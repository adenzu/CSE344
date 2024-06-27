// stats.c
#include <stdio.h>
#include <pthread.h>
#include "stats.h"

void stats_init(Stats *stats)
{
    stats->execution_time = 0.0;
    stats->regular_files = 0;
    stats->directories = 0;
    stats->bytes = 0;
    pthread_mutex_init(&stats->mutex, NULL);
}

void stats_print(Stats *stats)
{
    printf("Execution time: %.6f seconds\n", stats->execution_time);
    printf("Regular files: %d\n", stats->regular_files);
    printf("Directories: %d\n", stats->directories);
    printf("Bytes: %d\n", stats->bytes);
}

void stats_increment_regular_files(Stats *stats)
{
    pthread_mutex_lock(&stats->mutex);
    stats->regular_files++;
    pthread_mutex_unlock(&stats->mutex);
}

void stats_increment_directories(Stats *stats)
{
    pthread_mutex_lock(&stats->mutex);
    stats->directories++;
    pthread_mutex_unlock(&stats->mutex);
}

void stats_increment_bytes(Stats *stats, int bytes)
{
    pthread_mutex_lock(&stats->mutex);
    stats->bytes += bytes;
    pthread_mutex_unlock(&stats->mutex);
}
