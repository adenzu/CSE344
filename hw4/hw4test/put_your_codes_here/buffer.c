// buffer.c
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "transaction.h"
#include "buffer.h"

extern volatile sig_atomic_t stop;

void buffer_init(Buffer *buffer, int size)
{
    buffer->data = malloc(size * sizeof(Transaction));
    for (int i = 0; i < size; i++)
    {
        buffer->data[i].source_fd = -1;
        buffer->data[i].dest_fd = -1;
    }
    buffer->size = size;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->closed = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
}

void buffer_destroy(Buffer *buffer)
{
    free(buffer->data);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
}

void buffer_close(Buffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = 1;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

void buffer_put(Buffer *buffer, const Transaction *item)
{
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == buffer->size)
    {
        if (stop)
        {
            pthread_mutex_unlock(&buffer->mutex);
            return;
        }
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }
    buffer->data[buffer->tail].dest_fd = item->dest_fd;
    buffer->data[buffer->tail].source_fd = item->source_fd;
    buffer->tail = (buffer->tail + 1) % buffer->size;
    buffer->count++;
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

Transaction buffer_get(Buffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == 0)
    {
        if (stop || buffer->closed)
        {
            pthread_mutex_unlock(&buffer->mutex);
            return (Transaction){.source_fd = -1, .dest_fd = -1};
        }
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }
    Transaction item = buffer->data[buffer->head];
    buffer->head = (buffer->head + 1) % buffer->size;
    buffer->count--;
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    return item;
}
