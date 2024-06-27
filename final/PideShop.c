#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "circularqueue.h"

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 1024
#define POLL_TIMEOUT 1000
#define OVEN_CAPACITY 6
#define OVEN_APARATUS 3
#define OVEN_DOORS 2
#define MIN_ORDER_PREPARATION_TIME 3
#define MAX_ORDER_PREPARATION_TIME 5
#define ORDER_OVEN_TIME 3
#define DELIVERY_ORDER_COUNT 3

int connected = 0;

int ovenFree = OVEN_CAPACITY;
sem_t semOvenAparatus, semOvenDoors, semReadyOrders;
pthread_mutex_t mutexOven, mutexCookedOrders, mutexDeliveredOrders, mutexInDeliveryOrders;
pthread_mutex_t mutexOrdersWaitingForOven, mutexOrdersToBePrepared, mutexOrdersInPreparation;

typedef struct
{
    int socket;
    struct sockaddr_in address;
} client_t;

typedef struct
{
    int customerId;
    int p;
    int q;
} Order;

Order *order;

typedef struct
{
    Order *order;
    time_t time;
} OrderWithTime;

OrderWithTime *orderWithTime;

CircularQueue orders;
CircularQueue readyOrders;
CircularQueue ordersInDelivery;

int totalOrders = 1;
int cookedOrders = 0;
int ordersToBeDelivered = 0;
int ordersWaitingForOven = 0;
int ordersWaitingForDelivery = 0;
int ordersToBePrepared = 0;
int ordersInPreparation = 0;
int deliveredOrders = 0;
int ordersInDeliveryCount = 0;

int shopQ = 0;
int shopP = 0;

int deliverySpeed = 1;
int serverSocket;
int cookPoolSize, deliveryPoolSize;
pthread_t managerThread;
pthread_t *cookThreads;
pthread_t *courierThreads;
int *cookThreadsArgs;
int *courierThreadsArgs;
int logFd;

// --- utils ---
int randomRange(int min, int max)
{
    return rand() % (max - min + 1) + min;
}

char *getTimestamp()
{
    static char buffer[30];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

void cleanup();
void handle_sigint(int sig);

void setup_signal_handling()
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void handle_sigint(int sig)
{
    printf("Caught signal %d\n", sig);
    dprintf(logFd, "[%s] Caught signal %d\n", getTimestamp(), sig);

    cleanup();
    exit(EXIT_SUCCESS);
}

void cleanup()
{
    totalOrders = -1;

    printf("Cleaning up resources...\n");
    dprintf(logFd, "[%s] Cleaning up resources...\n", getTimestamp());

    if (!connected)
    {
        printf("Cleanup complete.\n");
        dprintf(logFd, "[%s] Cleanup complete.\n", getTimestamp());

        return;
    }

    // Cancel threads
    pthread_cancel(managerThread);
    for (int i = 0; i < cookPoolSize; i++)
    {
        pthread_cancel(cookThreads[i]);
    }
    for (int i = 0; i < deliveryPoolSize; i++)
    {
        pthread_cancel(courierThreads[i]);
    }

    // Join threads
    pthread_join(managerThread, NULL);
    for (int i = 0; i < cookPoolSize; i++)
    {
        pthread_join(cookThreads[i], NULL);
    }
    for (int i = 0; i < deliveryPoolSize; i++)
    {
        pthread_join(courierThreads[i], NULL);
    }

    // Free allocated memory
    free(order);
    free(cookThreads);
    free(orderWithTime);
    free(courierThreads);
    free(cookThreadsArgs);
    free(courierThreadsArgs);

    // Destroy semaphores and mutexes
    sem_destroy(&semOvenAparatus);
    sem_destroy(&semOvenDoors);
    sem_destroy(&semReadyOrders);
    pthread_mutex_destroy(&mutexOven);
    pthread_mutex_destroy(&mutexCookedOrders);
    pthread_mutex_destroy(&mutexDeliveredOrders);
    pthread_mutex_destroy(&mutexInDeliveryOrders);
    pthread_mutex_destroy(&mutexOrdersWaitingForOven);
    pthread_mutex_destroy(&mutexOrdersToBePrepared);
    pthread_mutex_destroy(&mutexOrdersInPreparation);

    // Clear circular queues
    clearCircularQueue(&orders);
    clearCircularQueue(&readyOrders);
    clearCircularQueue(&ordersInDelivery);

    // Close server socket
    close(serverSocket);

    printf("Cleanup complete.\n");
    dprintf(logFd, "[%s] Cleanup complete.\n", getTimestamp());

    close(logFd);
}

int pseudoInverse()
{
    // Could not implement calculation of the psuedo-inverse of a 30 by 40 matrix having complex elements
    time_t start = time(NULL);
    sleep(randomRange(MIN_ORDER_PREPARATION_TIME, MAX_ORDER_PREPARATION_TIME));
    time_t end = time(NULL);
    return (end - start) / 2;
}

int isReadGoingToBlock(client_t *client)
{
    struct pollfd pfd;
    pfd.fd = client->socket;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int ret = poll(&pfd, 1, POLL_TIMEOUT);
    if (ret == -1)
    {
        perror("poll");
        return 0;
    }
    if (ret == 0)
    {
        return 1;
    }
    if (pfd.revents & POLLIN)
    {
        return 0;
    }
    return 1;
}
// --- utils ---

// --- actions ---
void setOrdersToBePrepared(int count)
{
    pthread_mutex_lock(&mutexOrdersToBePrepared);
    ordersToBePrepared = count;
    pthread_mutex_unlock(&mutexOrdersToBePrepared);
}

void decreaseOrdersToBePrepared()
{
    pthread_mutex_lock(&mutexOrdersToBePrepared);
    ordersToBePrepared--;
    pthread_mutex_unlock(&mutexOrdersToBePrepared);
}

void increaseOrdersInPreparation()
{
    pthread_mutex_lock(&mutexOrdersInPreparation);
    ordersInPreparation++;
    pthread_mutex_unlock(&mutexOrdersInPreparation);
}

void decreaseOrdersInPreparation()
{
    pthread_mutex_lock(&mutexOrdersInPreparation);
    ordersInPreparation--;
    pthread_mutex_unlock(&mutexOrdersInPreparation);
}

void increaseCookedOrders()
{
    pthread_mutex_lock(&mutexCookedOrders);
    ordersToBeDelivered++;
    cookedOrders++;
    ordersWaitingForDelivery++;
    pthread_mutex_unlock(&mutexCookedOrders);
}

void decreaseCookedOrders()
{
    pthread_mutex_lock(&mutexCookedOrders);
    ordersToBeDelivered--;
    pthread_mutex_unlock(&mutexCookedOrders);
}

void decreaseOrdersWaitingForDelivery()
{
    pthread_mutex_lock(&mutexCookedOrders);
    ordersWaitingForDelivery--;
    pthread_mutex_unlock(&mutexCookedOrders);
}

void increaseDeliveredOrders()
{
    pthread_mutex_lock(&mutexDeliveredOrders);
    deliveredOrders++;
    pthread_mutex_unlock(&mutexDeliveredOrders);
}

void increaseOrdersInDelivery()
{
    pthread_mutex_lock(&mutexInDeliveryOrders);
    ordersInDeliveryCount++;
    pthread_mutex_unlock(&mutexInDeliveryOrders);
}

void decreaseOrdersInDelivery()
{
    pthread_mutex_lock(&mutexInDeliveryOrders);
    ordersInDeliveryCount--;
    pthread_mutex_unlock(&mutexInDeliveryOrders);
}

void increaseOrdersWaitingForOven()
{
    pthread_mutex_lock(&mutexOrdersWaitingForOven);
    ordersWaitingForOven++;
    pthread_mutex_unlock(&mutexOrdersWaitingForOven);
}

void decreaseOrdersWaitingForOven()
{
    pthread_mutex_lock(&mutexOrdersWaitingForOven);
    ordersWaitingForOven--;
    pthread_mutex_unlock(&mutexOrdersWaitingForOven);
}

void putInOven()
{
    pthread_mutex_lock(&mutexOven);
    ovenFree--;
    pthread_mutex_unlock(&mutexOven);
}

void takeFromOven()
{
    pthread_mutex_lock(&mutexOven);
    ovenFree++;
    pthread_mutex_unlock(&mutexOven);
}
// --- actions ---

// --- actors ---
void *manager(void *arg)
{
    client_t *client = (client_t *)arg;
    int buffer[3 * MAX_CLIENTS];
    int bytesRead = read(client->socket, buffer, sizeof(buffer));

    if (bytesRead == 0)
    {
        printf("Client disconnected\n");
        dprintf(logFd, "[%s] Client disconnected\n", getTimestamp());

        totalOrders = 0;
        return NULL;
    }

    if (bytesRead == -1)
    {
        perror("Could not read from socket");
        totalOrders = 0;
        return NULL;
    }

    int ordersCount = bytesRead / sizeof(int) / 3;
    totalOrders = ordersCount;
    setOrdersToBePrepared(ordersCount);
    for (int i = 0; i < bytesRead / sizeof(int); i += 3)
    {
        order = malloc(sizeof(Order));
        order->customerId = buffer[i + 0];
        order->p = buffer[i + 1];
        order->q = buffer[i + 2];
        enqueue(&orders, order);
        printf("Put order from client %d from (%d, %d) in queue\n", order->customerId, order->p, order->q);
        dprintf(logFd, "[%s] Put order from client %d from (%d, %d) in queue\n", getTimestamp(), order->customerId, order->p, order->q);
    }

    int previousOrdersWaitingForOven = 0;
    int previousOrdersInOven = 0;
    int previousOrdersToDeliver = 0;
    int previousOrdersInDelivery = 0;
    int previousDeliveredOrders = 0;
    int previousOrdersWaitingForDelivery = 0;
    int previousOrdersToBePrepared = 0;
    int previousOrdersInPreparation = 0;

    while (totalOrders > deliveredOrders)
    {
        if (
            previousOrdersInPreparation != ordersInPreparation ||
            previousOrdersToBePrepared != ordersToBePrepared ||
            previousOrdersWaitingForOven != ordersWaitingForOven ||
            previousOrdersInOven != OVEN_CAPACITY - ovenFree ||
            previousOrdersToDeliver != ordersToBeDelivered ||
            previousOrdersInDelivery != ordersInDeliveryCount ||
            previousOrdersWaitingForDelivery != ordersWaitingForDelivery ||
            previousDeliveredOrders != deliveredOrders)
        {
            printf("Number of orders waiting to be prepared: %d\n", ordersToBePrepared);
            dprintf(logFd, "[%s] Number of orders waiting to be prepared: %d\n", getTimestamp(), ordersToBePrepared);

            printf("Number of orders in preparation: %d\n", ordersInPreparation);
            dprintf(logFd, "[%s] Number of orders in preparation: %d\n", getTimestamp(), ordersInPreparation);

            printf("Number of orders waiting for oven: %d\n", ordersWaitingForOven);
            dprintf(logFd, "[%s] Number of orders waiting for oven: %d\n", getTimestamp(), ordersWaitingForOven);

            printf("Number of orders in oven: %d\n", OVEN_CAPACITY - ovenFree);
            dprintf(logFd, "[%s] Number of orders in oven: %d\n", getTimestamp(), OVEN_CAPACITY - ovenFree);

            printf("Number of orders waiting couriers: %d\n", ordersWaitingForDelivery);
            dprintf(logFd, "[%s] Number of orders waiting couriers: %d\n", getTimestamp(), ordersWaitingForDelivery);

            printf("Number of orders on couriers: %d\n", ordersInDeliveryCount);
            dprintf(logFd, "[%s] Number of orders on couriers: %d\n", getTimestamp(), ordersInDeliveryCount);

            printf("Number of delivered orders: %d\n", deliveredOrders);
            dprintf(logFd, "[%s] Number of delivered orders: %d\n", getTimestamp(), deliveredOrders);

            previousOrdersWaitingForOven = ordersWaitingForOven;
            previousOrdersInOven = OVEN_CAPACITY - ovenFree;
            previousOrdersToDeliver = ordersToBeDelivered;
            previousOrdersInDelivery = ordersInDeliveryCount;
            previousDeliveredOrders = deliveredOrders;
            previousOrdersWaitingForDelivery = ordersWaitingForDelivery;
            previousOrdersToBePrepared = ordersToBePrepared;
            previousOrdersInPreparation = ordersInPreparation;

            while (ordersToBeDelivered > 0)
            {
                decreaseCookedOrders();
                sem_post(&semReadyOrders);
            }
        }

        if (!isReadGoingToBlock(client))
        {
            char buffer[BUFFER_SIZE];
            int bytesRead = read(client->socket, buffer, sizeof(buffer));
            if (bytesRead == 0)
            {
                printf("Client disconnected\n");
                dprintf(logFd, "[%s] Client disconnected\n", getTimestamp());

                totalOrders = 0;
                return NULL;
            }

            if (bytesRead == -1)
            {
                perror("Could not read from socket");
                totalOrders = 0;
                return NULL;
            }

            if (strncmp(buffer, "CANCEL", 6) == 0)
            {
                printf("Client cancelled orders\n");
                dprintf(logFd, "[%s] Client cancelled orders\n", getTimestamp());

                totalOrders = 0;
                return NULL;
            }
        }

        sleep(1);
    }

    printf("All orders are cooked and delivered\n");
    dprintf(logFd, "[%s] All orders are cooked and delivered\n", getTimestamp());

    return NULL;
}

void *cook(void *arg)
{
    int id = *(int *)arg;

    CircularQueue ordersToPutInOven, ordersToTakeOutFromOven;
    initCircularQueue(&ordersToPutInOven);
    initCircularQueue(&ordersToTakeOutFromOven);

    int ordersCount = 0;

    while (cookedOrders < totalOrders)
    {
        Order *order = dequeue(&orders);

        if (order != NULL)
        {
            decreaseOrdersToBePrepared();
            increaseOrdersInPreparation();

            printf("Cook %d is preparing order for customer %d\n", id, order->customerId);
            dprintf(logFd, "[%s] Cook %d is preparing order for customer %d\n", getTimestamp(), id, order->customerId);

            int sleepTime = pseudoInverse();

            decreaseOrdersInPreparation();

            printf("Cook %d prepared order for customer %d\n", id, order->customerId);
            dprintf(logFd, "[%s] Cook %d prepared order for customer %d\n", getTimestamp(), id, order->customerId);

            orderWithTime = malloc(sizeof(OrderWithTime));
            orderWithTime->order = order;
            orderWithTime->time = sleepTime;
            enqueue(&ordersToPutInOven, orderWithTime);

            increaseOrdersWaitingForOven();
        }

        int ovenAvailable = sem_trywait(&semOvenDoors);

        if (ovenAvailable == -1 && errno == EAGAIN)
        {
            continue;
        }

        int paddleAvailable = sem_trywait(&semOvenAparatus);

        if (paddleAvailable == -1 && errno == EAGAIN)
        {
            sem_post(&semOvenDoors);
            continue;
        }

        OrderWithTime *firstOrder = peek(&ordersToTakeOutFromOven);
        next(&ordersToTakeOutFromOven);
        OrderWithTime *currentOrder = peek(&ordersToTakeOutFromOven);
        while (currentOrder != firstOrder)
        {
            if (currentOrder->time > time(NULL))
            {
                next(&ordersToTakeOutFromOven);
                currentOrder = peek(&ordersToTakeOutFromOven);
                continue;
            }

            pthread_mutex_lock(&mutexOven);
            ovenFree++;
            pthread_mutex_unlock(&mutexOven);

            currentOrder = dequeue(&ordersToTakeOutFromOven);
            enqueue(&readyOrders, currentOrder->order);

            printf("Cook %d put order for customer %d in the delivery queue\n", id, currentOrder->order->customerId);
            dprintf(logFd, "[%s] Cook %d put order for customer %d in the delivery queue\n", getTimestamp(), id, currentOrder->order->customerId);

            ordersCount++;

            increaseCookedOrders();

            free(currentOrder);
            next(&ordersToTakeOutFromOven);
        }

        if (currentOrder != NULL && currentOrder->time <= time(NULL))
        {
            pthread_mutex_lock(&mutexOven);
            ovenFree++;
            pthread_mutex_unlock(&mutexOven);

            currentOrder = dequeue(&ordersToTakeOutFromOven);
            enqueue(&readyOrders, currentOrder->order);

            printf("Cook %d put order for customer %d in the delivery queue\n", id, currentOrder->order->customerId);
            dprintf(logFd, "[%s] Cook %d put order for customer %d in the delivery queue\n", getTimestamp(), id, currentOrder->order->customerId);

            ordersCount++;

            increaseCookedOrders();

            free(currentOrder);
        }

        while (ovenFree > 0)
        {
            OrderWithTime *orderWithTime = dequeue(&ordersToPutInOven);

            if (orderWithTime == NULL)
            {
                break;
            }

            pthread_mutex_lock(&mutexOven);
            if (ovenFree == 0)
            {
                pthread_mutex_unlock(&mutexOven);
                enqueue(&ordersToPutInOven, orderWithTime);
                break;
            }
            ovenFree--;
            pthread_mutex_unlock(&mutexOven);

            decreaseOrdersWaitingForOven();

            orderWithTime->time += time(NULL);
            enqueue(&ordersToTakeOutFromOven, orderWithTime);

            printf("Cook %d put order for customer %d in the oven\n", id, orderWithTime->order->customerId);
            dprintf(logFd, "[%s] Cook %d put order for customer %d in the oven\n", getTimestamp(), id, orderWithTime->order->customerId);
        }

        sem_post(&semOvenAparatus);
        sem_post(&semOvenDoors);
    }

    printf("Cook %d is done\n", id);
    dprintf(logFd, "[%s] Cook %d is done\n", getTimestamp(), id);

    *(int *)arg = ordersCount;

    return NULL;
}

void *courier(void *arg)
{
    int id = *(int *)arg;

    CircularQueue myOrders;
    initCircularQueue(&myOrders);
    int myQ = 0;
    int myP = 0;
    int ordersCount = 0;
    while (deliveredOrders < totalOrders)
    {

        if (myOrders.size == DELIVERY_ORDER_COUNT || (totalOrders - deliveredOrders - ordersInDeliveryCount) < DELIVERY_ORDER_COUNT)
        {
            while (myOrders.size > 0)
            {
                Order *order = dequeue(&myOrders);

                printf("Courier %d is delivering order for customer %d to position (%d, %d)\n", id, order->customerId, order->p, order->q);
                dprintf(logFd, "[%s] Courier %d is delivering order for customer %d to position (%d, %d)\n", getTimestamp(), id, order->customerId, order->p, order->q);
                sleep((((order->p - myP) * (order->p - myP) + (order->q - myQ) * (order->q - myQ))) / (deliverySpeed * 10000));
                printf("Order for customer %d to position (%d, %d) is delivered\n", order->customerId, order->p, order->q);
                dprintf(logFd, "[%s] Order for customer %d to position (%d, %d) is delivered\n", getTimestamp(), order->customerId, order->p, order->q);

                myP = order->p;
                myQ = order->q;
                free(order);
                ordersCount++;
                decreaseOrdersInDelivery();
                increaseDeliveredOrders();
            }
            if (myP + myQ)
            {
                printf("Courier %d is returning to the shop\n", id);
                dprintf(logFd, "[%s] Courier %d is returning to the shop\n", getTimestamp(), id);

                sleep((myP * myP + myQ * myQ) / (deliverySpeed * 10000));
                printf("Courier %d returned to the shop\n", id);
                dprintf(logFd, "[%s] Courier %d returned to the shop\n", getTimestamp(), id);

                myP = 0;
                myQ = 0;
            }
        }

        int readyOrdersAvailable = sem_trywait(&semReadyOrders);

        if (readyOrdersAvailable == -1 && errno == EAGAIN)
        {
            continue;
        }

        Order *order = dequeue(&readyOrders);
        if (order != NULL)
        {
            decreaseOrdersWaitingForDelivery();
            enqueue(&myOrders, order);
            increaseOrdersInDelivery();
            printf("Courier %d is taking order for customer %d\n", id, order->customerId);
            dprintf(logFd, "[%s] Courier %d is taking order for customer %d\n", getTimestamp(), id, order->customerId);
        }
    }

    printf("Courier %d is done\n", id);
    dprintf(logFd, "[%s] Courier %d is done\n", getTimestamp(), id);

    if (myP + myQ)
    {
        printf("Courier %d is returning to the shop\n", id);
        dprintf(logFd, "[%s] Courier %d is returning to the shop\n", getTimestamp(), id);

        sleep((myP * myP + myQ * myQ) / (deliverySpeed * 10000));
        printf("Courier %d returned to the shop\n", id);
        dprintf(logFd, "[%s] Courier %d returned to the shop\n", getTimestamp(), id);

        myP = 0;
        myQ = 0;
    }

    *(int *)arg = ordersCount;

    return NULL;
}
// --- actors ---

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s [port] [cookPoolSize] [deliveryPoolSize] [deliverySpeed]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    cookPoolSize = atoi(argv[2]);
    deliveryPoolSize = atoi(argv[3]);
    deliverySpeed = atoi(argv[4]);

    if (port < 1024 || port > 65535)
    {
        fprintf(stderr, "Port must be in range 1024-65535\n");
        return 1;
    }

    if (cookPoolSize < 1 || cookPoolSize > 1024)
    {
        fprintf(stderr, "Cook pool size must be in range 1-1024\n");
        return 1;
    }

    if (deliveryPoolSize < 1 || deliveryPoolSize > 1024)
    {
        fprintf(stderr, "Delivery pool size must be in range 1-1024\n");
        return 1;
    }

    if (deliverySpeed < 1 || deliverySpeed > 1024)
    {
        fprintf(stderr, "Delivery speed must be in range 1-1024\n");
        return 1;
    }

    // Set up signal handling
    setup_signal_handling();

    logFd = open("server.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (logFd == -1)
    {
        perror("Failed to open log file");
        return 1;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == 0)
    {
        perror("Could not create socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Bind failed");
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, MAX_CLIENTS) == -1)
    {
        perror("Listen failed");
        close(serverSocket);
        return 1;
    }

    printf("Server listening on port %d\n", port);
    dprintf(logFd, "[%s] Server listening on port %d\n", getTimestamp(), port);

    while (1)
    {
        printf("Waiting new orders...\n");
        dprintf(logFd, "[%s] Waiting new orders...\n", getTimestamp());

        client_t client;
        socklen_t addrLen = sizeof(client.address);
        client.socket = accept(serverSocket, (struct sockaddr *)&client.address, &addrLen);
        if (client.socket == -1)
        {
            perror("Accept failed");
        }

        printf("Accepted orders from %s:%d\n", inet_ntoa(client.address.sin_addr), ntohs(client.address.sin_port));
        dprintf(logFd, "[%s] Accepted orders from %s:%d\n", getTimestamp(), inet_ntoa(client.address.sin_addr), ntohs(client.address.sin_port));

        printf("Shop is closed for new orders\n");
        dprintf(logFd, "[%s] Shop is closed for new orders\n", getTimestamp());

        connected = 1;

        totalOrders = 1;
        cookedOrders = 0;
        ordersToBeDelivered = 0;
        ordersWaitingForDelivery = 0;
        ordersWaitingForOven = 0;
        ordersToBePrepared = 0;
        deliveredOrders = 0;
        ordersInDeliveryCount = 0;

        sem_init(&semOvenAparatus, 0, OVEN_APARATUS);
        sem_init(&semOvenDoors, 0, OVEN_DOORS);
        sem_init(&semReadyOrders, 0, 0);
        pthread_mutex_init(&mutexOven, NULL);
        pthread_mutex_init(&mutexCookedOrders, NULL);
        pthread_mutex_init(&mutexDeliveredOrders, NULL);
        pthread_mutex_init(&mutexInDeliveryOrders, NULL);
        pthread_mutex_init(&mutexOrdersWaitingForOven, NULL);
        pthread_mutex_init(&mutexOrdersToBePrepared, NULL);
        pthread_mutex_init(&mutexOrdersInPreparation, NULL);

        initCircularQueue(&orders);
        initCircularQueue(&readyOrders);
        initCircularQueue(&ordersInDelivery);

        pthread_create(&managerThread, NULL, manager, &client);

        cookThreads = malloc(cookPoolSize * sizeof(pthread_t));
        cookThreadsArgs = malloc(cookPoolSize * sizeof(int));
        for (int i = 0; i < cookPoolSize; i++)
        {
            cookThreadsArgs[i] = i;
            pthread_create(&cookThreads[i], NULL, cook, &cookThreadsArgs[i]);
        }

        courierThreads = malloc(deliveryPoolSize * sizeof(pthread_t));
        courierThreadsArgs = malloc(deliveryPoolSize * sizeof(int));
        for (int i = 0; i < deliveryPoolSize; i++)
        {
            courierThreadsArgs[i] = i;
            pthread_create(&courierThreads[i], NULL, courier, &courierThreadsArgs[i]);
        }

        pthread_join(managerThread, NULL);
        for (int i = 0; i < cookPoolSize; i++)
        {
            pthread_join(cookThreads[i], NULL);
        }
        for (int i = 0; i < deliveryPoolSize; i++)
        {
            pthread_join(courierThreads[i], NULL);
        }

        printf("\nAll threads are done\n");
        dprintf(logFd, "[%s] All threads are done\n", getTimestamp());

        printf("Stats:\n");
        dprintf(logFd, "[%s] Stats:\n", getTimestamp());

        for (int i = 0; i < cookPoolSize; i++)
        {
            printf("Cook %d prepared %d orders\n", i, cookThreadsArgs[i]);
            dprintf(logFd, "[%s] Cook %d prepared %d orders\n", getTimestamp(), i, cookThreadsArgs[i]);
        }

        for (int i = 0; i < deliveryPoolSize; i++)
        {
            printf("Courier %d delivered %d orders\n", i, courierThreadsArgs[i]);
            dprintf(logFd, "[%s] Courier %d delivered %d orders\n", getTimestamp(), i, courierThreadsArgs[i]);
        }

        free(cookThreads);
        free(courierThreads);
        free(cookThreadsArgs);
        free(courierThreadsArgs);

        sem_destroy(&semOvenAparatus);
        sem_destroy(&semOvenDoors);
        sem_destroy(&semReadyOrders);
        pthread_mutex_destroy(&mutexOven);
        pthread_mutex_destroy(&mutexCookedOrders);
        pthread_mutex_destroy(&mutexDeliveredOrders);
        pthread_mutex_destroy(&mutexInDeliveryOrders);
        pthread_mutex_destroy(&mutexOrdersWaitingForOven);
        pthread_mutex_destroy(&mutexOrdersToBePrepared);
        pthread_mutex_destroy(&mutexOrdersInPreparation);

        clearCircularQueue(&orders);
        clearCircularQueue(&readyOrders);
        clearCircularQueue(&ordersInDelivery);

        printf("Done serving to %s:%d\n\n", inet_ntoa(client.address.sin_addr), ntohs(client.address.sin_port));
        dprintf(logFd, "[%s] Done serving to %s:%d\n\n", getTimestamp(), inet_ntoa(client.address.sin_addr), ntohs(client.address.sin_port));

        write(client.socket, "Thank you for your order\n", 26);
        close(client.socket);

        connected = 0;
    }

    close(serverSocket);
    close(logFd);

    return 0;
}
