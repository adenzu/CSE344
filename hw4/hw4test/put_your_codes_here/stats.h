// stats.h
#ifndef STATS_H
#define STATS_H

#include <pthread.h>

typedef struct
{
    double execution_time;
    int regular_files;
    int directories;
    int bytes;
    pthread_mutex_t mutex;
} Stats;

void stats_init(Stats *stats);
void stats_print(Stats *stats);
void stats_increment_regular_files(Stats *stats);
void stats_increment_directories(Stats *stats);
void stats_increment_bytes(Stats *stats, int bytes);

#endif // STATS_H