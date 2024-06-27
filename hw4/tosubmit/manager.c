// manager.c
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include "buffer.h"
#include "transaction.h"
#include "thread_args.h"

extern volatile sig_atomic_t stop;

void *manager_function(void *arg)
{
    ManagerThreadArgs *args = (ManagerThreadArgs *)arg;
    Buffer *buffer = args->buffer;
    char *source_dir = args->source_dir;
    char *dest_dir = args->dest_dir;

    DIR *dir = opendir(source_dir);
    if (dir == NULL)
    {
        perror("Failed to open source directory");
        return NULL;
    }

    struct dirent *entry;
    while (!stop && (entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, PATH_MAX, "%s/%s", source_dir, entry->d_name);
            snprintf(dest_path, PATH_MAX, "%s/%s", dest_dir, entry->d_name);

            int src_fd = open(src_path, O_RDONLY);
            if (src_fd < 0)
            {
                perror("Failed to open source file");
                continue;
            }

            int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dest_fd < 0)
            {
                perror("Failed to open/create destination file");
                close(src_fd);
                continue;
            }

            Transaction transaction = {src_fd, dest_fd};
            buffer_put(buffer, &transaction);
        }
        else if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, PATH_MAX, "%s/%s", source_dir, entry->d_name);
            snprintf(dest_path, PATH_MAX, "%s/%s", dest_dir, entry->d_name);

            if (mkdir(dest_path, 0755) < 0)
            {
                if (errno != EEXIST)
                {
                    perror("Failed to create destination directory");
                    continue;
                }
            }

            stats_increment_directories(args->stats);

            ManagerThreadArgs new_args;
            new_args.buffer = buffer;
            new_args.source_dir = src_path;
            new_args.dest_dir = dest_path;
            new_args.stats = args->stats;

            manager_function(&new_args);
        }
    }

    closedir(dir);
    return NULL;
}
