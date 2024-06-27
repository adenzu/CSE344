#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "circularqueue.h"

void initCircularQueue(CircularQueue *queue)
{
    queue->current = NULL;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

int isEmpty(CircularQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    int result = queue->size == 0;
    pthread_mutex_unlock(&queue->mutex);
    return result;
}

void *peek(CircularQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0)
    {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    void *data = queue->current->data;
    pthread_mutex_unlock(&queue->mutex);
    return data;
}

void next(CircularQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->size != 0)
    {
        queue->current = queue->current->next;
    }
    pthread_mutex_unlock(&queue->mutex);
}

void enqueue(CircularQueue *queue, void *data)
{
    pthread_mutex_lock(&queue->mutex);
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
    newNode->data = data;
    if (queue->size == 0)
    {
        newNode->next = newNode;
        queue->current = newNode;
    }
    else
    {
        newNode->next = queue->current->next;
        queue->current->next = newNode;
    }
    queue->size++;
    pthread_mutex_unlock(&queue->mutex);
}

void *dequeue(CircularQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0)
    {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    Node *node = queue->current->next;
    void *data = node->data;

    if (queue->current == node)
    {
        queue->current = NULL;
    }
    else
    {
        queue->current->next = node->next;
    }

    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    free(node);
    return data;
}

void clearCircularQueue(CircularQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size > 0)
    {
        Node *node = queue->current->next;
        queue->current->next = node->next;
        free(node->data);
        free(node);
        queue->size--;
    }
    queue->current = NULL;
    pthread_mutex_unlock(&queue->mutex);
}
