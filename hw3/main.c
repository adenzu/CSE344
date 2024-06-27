#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_PICKUP 4
#define MAX_AUTOMOBILE 8
#define MAX_QUEUE 1

int sleep_args[] = {10, 1, 1, 1, 1};

#define TIME_SLOWDOWN sleep_args[4]
#define CAR_OWNER_SLEEP sleep_args[0] * TIME_SLOWDOWN
#define CAR_OWNER_PRODUCER_SLEEP sleep_args[1] * TIME_SLOWDOWN
#define CAR_ATTENDANT_MOVE_SLEEP sleep_args[2] * TIME_SLOWDOWN
#define CAR_ATTENDANT_CHECK_SLEEP sleep_args[3] * TIME_SLOWDOWN

/**
 * @enum CarType
 * Represents the different types of cars.
 */
enum CarType
{
    Pickup,
    Automobile
};

/**
 * @enum ParkSuccess
 * @brief Represents the success status of a park operation.
 *
 * This enumeration is used to indicate the success status of a park operation.
 * It provides different values to represent different success scenarios.
 */
enum ParkSuccess
{
    Success,
    Fail
};

/**
 * @struct Node
 * @brief Represents a node in a data structure.
 *
 * This struct is used to define a node in a data structure.
 * It contains the necessary fields to store data and maintain
 * the structure of the data.
 */
struct Node
{
    enum CarType car;
    int owner_id;
    sem_t transaction;
    enum ParkSuccess park_success;
    struct Node *next;
};

// Define semaphore variables
sem_t pickup_empty_spots, pickup_full_spots;
sem_t auto_empty_spots, auto_full_spots;
sem_t car_in_queue_mutex, car_out_queue_mutex;

struct Node *car_in_queue = NULL;
struct Node *car_out_queue = NULL;

int parked_pickup = 0;
int parked_automobile = 0;

int car_owner_threads_exist = 1;

/**
 * Enqueues a new node with the specified car type and owner ID at the end of the linked list.
 *
 * @param head A pointer to the head pointer of the linked list.
 * @param car The car type to be enqueued.
 * @param owner_id The ID of the owner of the car.
 * @return A pointer to the newly created node.
 */
struct Node *enqueue(struct Node **head, enum CarType car, int owner_id)
{
    struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
    new_node->car = car;
    new_node->owner_id = owner_id;
    new_node->next = NULL;

    sem_init(&new_node->transaction, 0, 0);

    if (*head == NULL)
    {
        *head = new_node;
        return new_node;
    }

    struct Node *last = *head;
    while (last->next != NULL)
    {
        last = last->next;
    }

    last->next = new_node;

    return new_node;
}

/**
 * Removes and returns the first node from the linked list.
 *
 * @param head A pointer to the head of the linked list.
 * @return A pointer to the removed node, or NULL if the list is empty.
 */
struct Node *dequeue(struct Node **head)
{
    if (*head == NULL)
    {
        return NULL;
    }

    struct Node *temp = *head;
    *head = (*head)->next;
    return temp;
}

/**
 * Calculates the number of cars in the queue.
 *
 * This function takes a pointer to the head of a linked list and counts the number of nodes in the list,
 * representing the number of cars in the queue.
 *
 * @param head A pointer to the head of the linked list.
 * @return The number of cars in the queue.
 */
int cars_in_queue(struct Node *head)
{
    int count = 0;
    struct Node *current = head;
    while (current != NULL)
    {
        count++;
        current = current->next;
    }

    return count;
}

/**
 * @brief Represents a thread function that simulates a car owner.
 *
 * This function is used as the entry point for a thread that represents a car owner.
 * It takes a void pointer as an argument, which can be used to pass any necessary data to the thread.
 * The function does not return a value.
 *
 * @param arg A void pointer that can be used to pass any necessary data to the thread.
 */
void *car_owner(void *arg)
{
    int owner_id = *(int *)arg;

    printf("Car owner %d arrived.\n", owner_id);

    enum CarType car = rand() % 2 == 0 ? Pickup : Automobile;

    sem_wait(&car_in_queue_mutex);
    int car_count = cars_in_queue(car_in_queue);
    if (car_count < MAX_QUEUE)
    {
        struct Node *queued_car = enqueue(&car_in_queue, car, owner_id);
        sem_post(&car_in_queue_mutex);
        printf("Car owner %d is waiting in their %s in temporary parking lot. Cars in temporary park: %d/%d\n", owner_id, car == Pickup ? "pickup" : "automobile", car_count + 1, MAX_QUEUE);
        sem_wait(&queued_car->transaction);
        if (queued_car->park_success == Fail)
        {
            printf("Car owner %d left because there is no empty spot in the parking lot.\n", owner_id);
            free(queued_car);
            return NULL;
        }
        free(queued_car);
    }
    else
    {
        printf("Car owner %d arrived but left because the temporary parking lot is full. Cars in temporary park: %d/%d\n", owner_id, car_count, MAX_QUEUE);
        sem_post(&car_in_queue_mutex);
        return NULL;
    }

    sleep(CAR_OWNER_SLEEP);

    printf("Car owner %d returned to pick up their car.\n", owner_id);

    sem_wait(&car_out_queue_mutex);
    struct Node *queued_car = enqueue(&car_out_queue, car, owner_id);
    sem_post(&car_out_queue_mutex);

    sem_wait(&queued_car->transaction);
    free(queued_car);

    return NULL;
}

/**
 * @brief This function is responsible for producing car owners.
 *
 * @param arg A pointer to the argument passed to the thread function.
 * @return void* The return value of the thread function.
 */
void *car_owner_producer(void *arg)
{
    int car_owners = *(int *)arg;
    pthread_t *car_owner_threads = (pthread_t *)calloc(car_owners, sizeof(pthread_t));
    int *owner_ids = (int *)calloc(car_owners, sizeof(int));
    for (int i = 0; i < car_owners; i++)
    {
        owner_ids[i] = i;
        pthread_create(&car_owner_threads[i], NULL, car_owner, &owner_ids[i]);
        sleep(CAR_OWNER_PRODUCER_SLEEP);
    }
    for (int i = 0; i < car_owners; i++)
    {
        pthread_join(car_owner_threads[i], NULL);
    }
    free(car_owner_threads);
    free(owner_ids);
    return NULL;
}

/**
 * Moves the attendant to a new parking lot.
 *
 * @param attendant_at_parking_lot Pointer to the current parking lot where the attendant is located.
 */
void attendant_moves(int *attendant_at_parking_lot)
{
    if (*attendant_at_parking_lot)
    {
        printf("\tCar attendant is moving to the temporary parking lot.\n");
        *attendant_at_parking_lot = 0;
    }
    else
    {
        printf("\tCar attendant is moving to the parking lot.\n");
        *attendant_at_parking_lot = 1;
    }
    sleep(CAR_ATTENDANT_MOVE_SLEEP);
}

/**
 * Car Attendant Function
 *
 * This function is responsible for handling the tasks of a car attendant.
 * It takes a void pointer as an argument and returns a void pointer.
 *
 * @param arg - A void pointer representing any additional arguments passed to the function.
 * @return void* - A void pointer representing the result of the function.
 */
void *car_attendant(void *arg)
{
    int attendant_at_parking_lot = 1;
    while (car_owner_threads_exist)
    {
        sleep(CAR_ATTENDANT_CHECK_SLEEP);

        printf("\tCar attendant is checking if any car owner is here to pick up their car.\n");
        sem_wait(&car_out_queue_mutex);
        if (car_out_queue != NULL)
        {
            struct Node *car = dequeue(&car_out_queue);
            sem_post(&car_out_queue_mutex);

            printf("\tCar attendant validated there is a car owner here to pick up their car.\n");

            if (!attendant_at_parking_lot)
            {
                attendant_moves(&attendant_at_parking_lot);
            }

            printf("\tOwner %d is here to pick up their %s.\n", car->owner_id, car->car == Pickup ? "pickup" : "automobile");
            printf("\tCar attendant is bringing the car to the owner.\n");

            attendant_moves(&attendant_at_parking_lot);

            if (car->car == Pickup)
            {
                sem_wait(&pickup_full_spots);
                printf("\tOwner %d picked up their pickup car.\n", car->owner_id);
                parked_pickup--;
                sem_post(&pickup_empty_spots);
            }
            else
            {
                sem_wait(&auto_full_spots);
                printf("\tOwner %d picked up their automobile.\n", car->owner_id);
                parked_automobile--;
                sem_post(&auto_empty_spots);
            }
            sem_post(&car->transaction);
        }
        else
        {
            printf("\tNo car owner is here to pick up their car.\n");
            sem_post(&car_out_queue_mutex);
        }

        printf("\tCar attendant is checking if any car is in temporary parking lot.\n");
        sem_wait(&car_in_queue_mutex);
        if (car_in_queue != NULL)
        {
            struct Node *car = dequeue(&car_in_queue);
            sem_post(&car_in_queue_mutex);

            printf("\tCar attendant validated there is a car in temporary parking lot.\n");

            if (attendant_at_parking_lot)
            {
                attendant_moves(&attendant_at_parking_lot);
            }

            printf("\tCar attendant is bringing the %s of the owner %d to the parking lot.\n", car->car == Pickup ? "pickup" : "automobile", car->owner_id);
            attendant_moves(&attendant_at_parking_lot);

            if (car->car == Pickup)
            {
                if (parked_pickup >= MAX_PICKUP)
                {
                    car->park_success = Fail;
                    sem_post(&car->transaction);
                    printf("\tNo empty spot for pickup car. Attendant could not park the car.\n");
                    printf("\tAttendant is returning the pickup car to the owner.\n");
                    attendant_moves(&attendant_at_parking_lot);
                    printf("\tParking lot status: Pickup: %d/%d, Automobile: %d/%d\n", parked_pickup, MAX_PICKUP, parked_automobile, MAX_AUTOMOBILE);
                    continue;
                }
                car->park_success = Success;
                sem_post(&car->transaction);

                sem_wait(&pickup_empty_spots);
                printf("\tCar attendant parked the pickup car.\n");
                parked_pickup++;
                sem_post(&pickup_full_spots);
            }
            else
            {
                if (parked_automobile >= MAX_AUTOMOBILE)
                {
                    car->park_success = Fail;
                    sem_post(&car->transaction);
                    printf("\tNo empty spot for automobile. Attendant could not park the car.\n");
                    printf("\tAttendant is returning the automobile car to the owner.\n");
                    attendant_moves(&attendant_at_parking_lot);
                    printf("\tParking lot status: Pickup: %d/%d, Automobile: %d/%d\n", parked_pickup, MAX_PICKUP, parked_automobile, MAX_AUTOMOBILE);
                    continue;
                }
                car->park_success = Success;
                sem_post(&car->transaction);

                sem_wait(&auto_empty_spots);
                printf("\tCar attendant parked the automobile.\n");
                parked_automobile++;
                sem_post(&auto_full_spots);
            }
        }
        else
        {
            printf("\tNo car is in temporary parking lot.\n");
            sem_post(&car_in_queue_mutex);
        }

        printf("\tParking lot status: Pickup: %d/%d, Automobile: %d/%d\n", parked_pickup, MAX_PICKUP, parked_automobile, MAX_AUTOMOBILE);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int car_owners = 100;

    if (argc == 2 && atoi(argv[1]) > 0)
    {
        car_owners = atoi(argv[1]);
    }
    else if (argc == 2)
    {
        printf("Invalid argument. Please provide a positive integer as the number of car owners.\n");
        return 1;
    }
    else if (argc > 2)
    {
        printf("Too many arguments. Please provide only one argument as the number of car owners.\n");
        return 1;
    }

    srand(time(NULL));

    sem_init(&pickup_empty_spots, 0, MAX_PICKUP);
    sem_init(&pickup_full_spots, 0, 0);

    sem_init(&auto_empty_spots, 0, MAX_AUTOMOBILE);
    sem_init(&auto_full_spots, 0, 0);

    sem_init(&car_in_queue_mutex, 0, 1);
    sem_init(&car_out_queue_mutex, 0, 1);

    pthread_t car_owner_producer_thread;
    pthread_create(&car_owner_producer_thread, NULL, car_owner_producer, &car_owners);

    pthread_t car_attendant_thread;
    pthread_create(&car_attendant_thread, NULL, car_attendant, NULL);

    pthread_join(car_owner_producer_thread, NULL);
    car_owner_threads_exist = 0;
    pthread_join(car_attendant_thread, NULL);

    printf("\nSimulation completed.\n");

    return 0;
}
