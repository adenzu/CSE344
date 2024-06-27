// buffer.h
#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include "transaction.h"

typedef struct
{
    Transaction *data;
    int size;
    int count;
    int head;
    int tail;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Buffer;

void buffer_init(Buffer *buffer, int size);
void buffer_destroy(Buffer *buffer);
void buffer_close(Buffer *buffer);
void buffer_put(Buffer *buffer, const Transaction *item);
Transaction buffer_get(Buffer *buffer);

#endif // BUFFER_H
