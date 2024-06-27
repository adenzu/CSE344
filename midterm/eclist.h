#ifndef EC_LIST_H
#define EC_LIST_H

#include <stdlib.h>

struct node
{
    int value;
    struct node *previous;
    struct node *next;
};

struct double_linkedlist
{
    int size;
    struct node *head;
    struct node *tail;
};

struct double_linkedlist *create_double_linkedlist()
{
    struct double_linkedlist *new_double_linkedlist = (struct double_linkedlist *)malloc(sizeof(struct double_linkedlist));
    new_double_linkedlist->size = 0;
    new_double_linkedlist->head = NULL;
    new_double_linkedlist->tail = NULL;
    return new_double_linkedlist;
}

void free_double_linkedlist(struct double_linkedlist *list)
{
    if (list == NULL)
    {
        return;
    }
    struct node *current = list->head;
    struct node *next;
    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }
    free(list);
}

struct node *create_node(int value, struct node *previous, struct node *next)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    new_node->value = value;
    new_node->previous = previous;
    new_node->next = next;
    return new_node;
}

int is_empty(const struct double_linkedlist *list)
{
    return list->size == 0;
}

int peek_head(const struct double_linkedlist *list)
{
    return list->head->value;
}

int peek_tail(const struct double_linkedlist *list)
{
    return list->tail->value;
}

void insert_head(struct double_linkedlist *list, int value)
{
    if (list->size == 0)
    {
        list->head = create_node(value, NULL, NULL);
        list->tail = list->head;
    }
    else
    {
        list->head = create_node(value, NULL, list->head);
        list->head->next->previous = list->head;
    }
    list->size++;
}

void insert_tail(struct double_linkedlist *list, int value)
{
    if (list->size == 0)
    {
        list->head = create_node(value, NULL, NULL);
        list->tail = list->head;
    }
    else
    {
        list->tail = create_node(value, list->tail, NULL);
        list->tail->previous->next = list->tail;
    }
    list->size++;
}

int pop_head(struct double_linkedlist *list)
{
    if (list->size == 0)
    {
        return 0;
    }
    struct node *popped = list->head;
    if (list->size == 1)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    else
    {
        list->head = list->head->next;
        list->head->previous = NULL;
    }
    list->size--;
    int popped_value = popped->value;
    free(popped);
    return popped_value;
}

int pop_tail(struct double_linkedlist *list)
{
    if (list->size == 0)
    {
        return 0;
    }
    struct node *popped = list->tail;
    if (list->size == 1)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    else
    {
        list->tail = list->tail->previous;
        list->tail->next = NULL;
    }
    list->size--;
    int popped_value = popped->value;
    free(popped);
    return popped_value;
}

#endif // EC_LIST_H