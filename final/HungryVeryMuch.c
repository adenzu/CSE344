#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define SERVER_READ_BUFFER_SIZE 1024

int sentOrders = 0;
int ordersCancelled = 0;

void sigintHandler(int signal)
{
    if (sentOrders == 0)
    {
        printf("No orders sent\n");
        exit(EXIT_SUCCESS);
    }
    ordersCancelled = 1;
    printf("Orders cancelled\n");
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = sigintHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s [port] [numberOfCustomers] [p] [q]\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    int port = atoi(argv[1]);
    int numberOfCustomers = atoi(argv[2]);
    int p = atoi(argv[3]);
    int q = atoi(argv[4]);

    if (port < 1024 || port > 65535)
    {
        fprintf(stderr, "Port must be in range 1024-65535\n");
        return 1;
    }

    if (numberOfCustomers < 1 || numberOfCustomers > 1024) // 1-1024
    {
        fprintf(stderr, "Number of customers must be in range 1-1024\n");
        return 1;
    }

    if (p < 1 || p > 1000) // 1-1000
    {
        fprintf(stderr, "p must be in range 1-1000\n");
        return 1;
    }

    if (q < 1 || q > 1000) // 1-1000
    {
        fprintf(stderr, "q must be in range 1-1000\n");
        return 1;
    }

    int connectionSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (connectionSocket == -1)
    {
        perror("Could not create socket");
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(connectionSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Connect failed");
        close(connectionSocket);
        return 1;
    }

    sentOrders = 1;

    int buffer[3 * numberOfCustomers];
    for (int i = 0; i < numberOfCustomers; i++)
    {
        buffer[3 * i + 0] = i;
        buffer[3 * i + 1] = rand() % p;
        buffer[3 * i + 2] = rand() % q;
        printf("Put order from customer %d at position (%d, %d) in queue\n", i, buffer[3 * i + 1], buffer[3 * i + 2]);
    }

    if (!ordersCancelled)
    {
        write(connectionSocket, buffer, sizeof(buffer));
    }

    int connectionSafelyClosed = 0;

    char serverResponse[SERVER_READ_BUFFER_SIZE];
    int bytesRead;
    while (!ordersCancelled && (bytesRead = read(connectionSocket, serverResponse, sizeof(serverResponse))) > 0)
    {
        for (int i = 0; i < bytesRead; i++)
        {
            printf("%c", serverResponse[i]);
        }
        if (strcmp(serverResponse, "Thank you for your order\n") == 0)
        {
            connectionSafelyClosed = 1;
            break;
        }
    }

    if (!ordersCancelled && (bytesRead == -1 || !connectionSafelyClosed))
    {
        printf("Shop burned down\n");
        close(connectionSocket);
        return 1;
    }

    if (ordersCancelled)
    {
        write(connectionSocket, "CANCEL", 6);
    }

    close(connectionSocket);

    return 0;
}
