// thread_args.h
#ifndef THREAD_ARGS_H
#define THREAD_ARGS_H

#include "buffer.h"
#include "stats.h"

typedef struct
{
    Buffer *buffer;
    char *source_dir;
    char *dest_dir;
    Stats *stats;
} ManagerThreadArgs;

typedef struct
{
    Buffer *buffer;
    Stats *stats;
} WorkerThreadArgs;

#endif // THREAD_ARGS_H