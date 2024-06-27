#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "shared.h"

char decline_reason_response[MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH] = "\0";

int is_client_alive(int client_pid)
{
    char server_to_client_fifo_path[MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH] = "\0";
    return does_fifo_exist(get_server_to_client_fifo_path(client_pid, server_to_client_fifo_path));
}

int open_server_to_client_fifo(int client_pid)
{
    char server_to_client_fifo_path[MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH] = "\0";
    int server_to_client_fifo_fd = open(get_server_to_client_fifo_path(client_pid, server_to_client_fifo_path), O_WRONLY);
    if (server_to_client_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return server_to_client_fifo_fd;
}

int decline_client(int client_pid)
{
    int server_to_client_fifo_fd = open_server_to_client_fifo(client_pid);
    if (server_to_client_fifo_fd == -1)
    {
        return -1;
    }

    int response_size = strlen(decline_reason_response) * sizeof(char);
    while (write(server_to_client_fifo_fd, decline_reason_response, response_size) == -1 && errno == EINTR)
        ;

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <client_pid> <decline_reason>\n", argv[0]);
        return 1;
    }

    char *client_id_string = argv[1];

    int client_pid = atoi(client_id_string);
    if (client_pid <= 0)
    {
        printf("Invalid client ID\n");
        return 1;
    }

    if (!is_client_alive(client_pid))
    {
        return 0;
    }

    char *decline_reason_string = argv[2];
    int decline_reason = atoi(decline_reason_string);
    get_decline_reason_response(decline_reason, decline_reason_response);
    if (!is_decline_reason_string_valid(decline_reason_response))
    {
        return 1;
    }

    if (decline_client(client_pid) == -1)
    {
        return 1;
    }

    return 0;
}