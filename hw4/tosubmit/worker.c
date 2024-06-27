// worker.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "buffer.h"
#include "transaction.h"
#include "thread_args.h"
#include "stats.h"

#define BUFFER_SIZE 4096

extern volatile sig_atomic_t stop;
extern pthread_barrier_t barrier;

void *worker_function(void *arg)
{
    WorkerThreadArgs *args = (WorkerThreadArgs *)arg;
    Buffer *buffer = args->buffer;
    Stats *stats = args->stats;
    char buf[BUFFER_SIZE];

    printf("Worker %ld initialized. Waiting for other workers to initalize...\n", pthread_self());
    pthread_barrier_wait(&barrier);
    printf("Worker %ld starting to copy files\n", pthread_self());

    while (!stop)
    {
        Transaction transaction = buffer_get(buffer);
        int src_fd = transaction.source_fd;
        int dest_fd = transaction.dest_fd;

        if (src_fd == -1 && dest_fd == -1)
        {
            break;
        }

        ssize_t bytes_read;
        while ((bytes_read = read(src_fd, buf, sizeof(buf))) > 0)
        {
            write(dest_fd, buf, bytes_read);
            stats_increment_bytes(stats, bytes_read);
        }

        close(src_fd);
        close(dest_fd);

        stats_increment_regular_files(stats);
    }

    if (stop)
    {
        printf("Worker %ld interrupted\n", pthread_self());
        pthread_barrier_wait(&barrier);
        printf("Worker %ld terminated\n", pthread_self());
    }
    else
    {
        printf("Worker %ld completed copying. Waiting for other workers to complete...\n", pthread_self());
        pthread_barrier_wait(&barrier);
        printf("Worker %ld exiting\n", pthread_self());
    }

    return NULL;
}
