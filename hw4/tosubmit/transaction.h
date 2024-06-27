// transaction.h
#ifndef TRANSACTION_H
#define TRANSACTION_H

typedef struct
{
    int source_fd;
    int dest_fd;
} Transaction;

void transaction_init(Transaction *transaction, int source_fd, int dest_fd);

#endif // TRANSACTION_H