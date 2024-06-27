// transaction.
#include "transaction.h"

void transaction_init(Transaction *transaction, int source_fd, int dest_fd)
{
    transaction->source_fd = source_fd;
    transaction->dest_fd = dest_fd;
}