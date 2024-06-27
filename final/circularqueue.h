#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <pthread.h>

typedef struct Node
{
    void *data;
    struct Node *next;
} Node;

typedef struct
{
    Node *current;
    int size;
    pthread_mutex_t mutex;
} CircularQueue;

void initCircularQueue(CircularQueue *queue);
int isEmpty(CircularQueue *queue);
void *peek(CircularQueue *queue);
void next(CircularQueue *queue);
void enqueue(CircularQueue *queue, void *data);
void *dequeue(CircularQueue *queue);
void clearCircularQueue(CircularQueue *queue);

#endif /* CIRCULARQUEUE_H */