// main.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "buffer.h"
#include "stats.h"
#include "thread_args.h"

#define MAX_BUFFER_SIZE 512

void *manager_function(void *arg);
void *worker_function(void *arg);
void print_usage();
void handle_sigint(int sig);

Buffer buffer;
volatile sig_atomic_t stop = 0;
pthread_barrier_t barrier;

void handle_sigint(int sig)
{
    if (stop)
    {
        // Exit immediately if SIGINT is received again
        exit(EXIT_FAILURE);
    }
    stop = 1;
    buffer_close(&buffer);
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_cond_broadcast(&buffer.not_full);
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char *source_dir = argv[3];
    char *dest_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    if (buffer_size > MAX_BUFFER_SIZE)
    {
        printf("Buffer size cannot exceed %d\n", MAX_BUFFER_SIZE);
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    pthread_t manager_thread;
    pthread_t *worker_threads = malloc(num_workers * sizeof(pthread_t));
    if (worker_threads == NULL)
    {
        perror("Failed to allocate memory for worker threads");
        return EXIT_FAILURE;
    }

    buffer_init(&buffer, buffer_size);

    Stats stats;
    stats_init(&stats);

    ManagerThreadArgs manager_args;
    manager_args.buffer = &buffer;
    manager_args.source_dir = source_dir;
    manager_args.dest_dir = dest_dir;
    manager_args.stats = &stats;

    pthread_barrier_init(&barrier, NULL, num_workers);

    clock_t start = clock();

    pthread_create(&manager_thread, NULL, manager_function, (void *)&manager_args);

    WorkerThreadArgs worker_args;
    worker_args.buffer = &buffer;
    worker_args.stats = &stats;

    for (int i = 0; i < num_workers; i++)
    {
        pthread_create(&worker_threads[i], NULL, worker_function, (void *)&worker_args);
    }

    pthread_join(manager_thread, NULL);
    printf("Manager thread completed\n");
    buffer_close(&buffer);

    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }

    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;

    buffer_destroy(&buffer);
    free(worker_threads);
    pthread_barrier_destroy(&barrier);

    stats.execution_time = elapsed_time;

    printf("Copy statistics are as follows:\n");
    stats_print(&stats);

    return EXIT_SUCCESS;
}

void print_usage()
{
    printf("Usage: MWCp <buffer_size> <num_workers> <source_dir> <dest_dir>\n");
}
