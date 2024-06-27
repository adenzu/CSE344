#ifndef SYSTEM_MIDTERM_SHARED_H
#define SYSTEM_MIDTERM_SHARED_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define PID_STRING_LENGTH 16

#define MAX_FILE_NAME_LENGTH 256

#define SERVER_LOG_CONNECTION_START "=== Connection Start ==="
#define SERVER_LOG_CONNECTION_END "=== Connection End ==="

#define CONNECTION_REQUEST_MUTEX_PATH_PREFIX "/system_midterm_connection_request_mutex_"
#define ARCHIVE_MUTEX_PATH_PREFIX "/system_midterm_archive_mutex_"
#define FILE_DATA_MUTEX_PATH_PREFIX "/system_midterm_file_data_mutex_"

#define MAX_LOG_FILE_BUFFER_SIZE 1024
#define MAX_LOG_FILE_PATH_LENGTH 256

#define SERVER_FIFO_BUFFER_SIZE 1024
#define MAX_SERVER_FIFO_PATH_LENGTH 256
#define SERVER_FIFO_PATH_PREFIX "/tmp/system_midterm_server_fifo_"

#define SERVER_TO_CLIENT_FIFO_BUFFER_SIZE 1024
#define MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH 256
#define SERVER_TO_CLIENT_FIFO_PATH_PREFIX "/tmp/system_midterm_server_to_client_fifo_"
#define SERVER_TO_CLIENT_DATA_FIFO_PATH_PREFIX "/tmp/system_midterm_server_to_client_data_fifo_"

#define CLIENT_TO_SERVER_FIFO_BUFFER_SIZE 1024
#define MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH 256
#define CLIENT_TO_SERVER_FIFO_PATH_PREFIX "/tmp/system_midterm_client_to_server_fifo_"
#define CLIENT_TO_SERVER_DATA_FIFO_PATH_PREFIX "/tmp/system_midterm_client_to_server_data_fifo_"

#define MAX_CONNECTION_REQUEST_LENGTH 256
#define CLIENT_CONNECTION_REQUEST_PREFIX "cr"

#define CLIENT_CONNECTION_REQUEST_TYPE_FLAG_LENGTH 1
#define CLIENT_CONNECTION_REQUEST_BLOCK "b"
#define CLIENT_CONNECTION_REQUEST_NONBLOCK "n"

#define MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH 1024
#define SERVER_TO_CLIENT_RESPONSE_CONNECTION_ACCEPTED "ca"
#define SERVER_TO_CLIENT_RESPONSE_KILL_BY_CAPACITY "kbc"
#define SERVER_TO_CLIENT_RESPONSE_KILL_BY_SERVER_TERMINATED "st"

#define INVALID_DECLINE_REASON "idr"

#define MAX_DECLINE_REASON_NUMBER_DIGITS 3
#define DECLINE_REASON_CAPACITY 1
#define DECLINE_REASON_SERVER_TERMINATED 2

#define MAX_CLIENT_COMMAND_ARGUMENT_COUNT 8
#define MAX_CLIENT_COMMAND_ARGUMENT_LENGTH 256
#define CLIENT_COMMAND_STRING_HELP "help"
#define CLIENT_COMMAND_STRING_LIST "list"
#define CLIENT_COMMAND_STRING_READF "readF"
#define CLIENT_COMMAND_STRING_WRITET "writeT"
#define CLIENT_COMMAND_STRING_UPLOAD "upload"
#define CLIENT_COMMAND_STRING_DOWNLOAD "download"
#define CLIENT_COMMAND_STRING_ARCHSERVER "archServer"
#define CLIENT_COMMAND_STRING_KILLSERVER "killServer"
#define CLIENT_COMMAND_STRING_QUIT "quit"

int check_file_exists(const char *file_path)
{
    if (access(file_path, F_OK) == -1)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        return -1;
    }
    return 1;
}

int check_file_exists_relative(int dirfd, const char *file_path)
{
    if (faccessat(dirfd, file_path, F_OK, 0) == -1)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        return -1;
    }
    return 1;
}

const char *get_filename(const char *filepath)
{
    const char *filename = NULL;
    for (const char *ptr = filepath; *ptr; ++ptr)
    {
        if (*ptr == '/' || *ptr == '\\')
        {
            filename = ptr + 1;
        }
    }
    return (filename) ? filename : filepath;
}

const char *get_extension(const char *filename)
{
    const char *extension = NULL;
    for (const char *ptr = filename; *ptr; ++ptr)
    {
        if (*ptr == '.')
        {
            extension = ptr + 1;
        }
    }
    return (extension) ? extension : filename;
}

char *get_filename_without_extension(const char *filepath, char *buffer)
{
    const char *filename = get_filename(filepath);
    const char *extension = get_extension(filename);
    int length = extension - filename - 1;
    strncpy(buffer, filename, length);
    buffer[length] = '\0';
    return buffer;
}

int find_valid_name(const char *file_path, char *buffer)
{
    if (check_file_exists(file_path) == 0)
    {
        sprintf(buffer, "%s", file_path);
        return 0;
    }

    char file_name[MAX_FILE_NAME_LENGTH];
    get_filename_without_extension(file_path, file_name);
    const char *file_extension = get_extension(file_path);

    const int limit = 100;
    for (int i = 1; i < limit; i++)
    {
        sprintf(buffer, "%s(%d).%s", file_name, i, file_extension);
        if (check_file_exists(buffer) == 0)
        {
            return 0;
        }
    }

    return -1;
}

int find_valid_name_relative(int dirfd, const char *file_path, char *buffer)
{
    if (check_file_exists_relative(dirfd, file_path) == 0)
    {
        sprintf(buffer, "%s", file_path);
        return 0;
    }

    char file_name[MAX_FILE_NAME_LENGTH];
    get_filename_without_extension(file_path, file_name);
    const char *file_extension = get_extension(file_path);

    const int limit = 100;
    for (int i = 1; i < limit; i++)
    {
        sprintf(buffer, "%s(%d).%s", file_name, i, file_extension);
        if (check_file_exists_relative(dirfd, buffer) == 0)
        {
            return 0;
        }
    }

    return -1;
}

char **allocate_command_array()
{
    char **command_array = (char **)calloc(MAX_CLIENT_COMMAND_ARGUMENT_COUNT, MAX_CLIENT_COMMAND_ARGUMENT_LENGTH * sizeof(char));
    for (int i = 0; i < MAX_CLIENT_COMMAND_ARGUMENT_COUNT; i++)
    {
        command_array[i] = (char *)calloc(MAX_CLIENT_COMMAND_ARGUMENT_LENGTH, sizeof(char));
    }
    return command_array;
}

void free_command_array(char **command_array)
{
    for (int i = 0; i < MAX_CLIENT_COMMAND_ARGUMENT_COUNT; i++)
    {
        free(command_array[i]);
    }
    free(command_array);
}

int are_strings_the_same(const char *string1, const char *string2)
{
    return strcmp(string1, string2) == 0;
}

char **parse_client_command(const char *command, char **parsed_command)
{
    int in_quotes = 0;
    int after_space = 0;
    int i = 0, j = 0, k = 0;
    for (; command[i] == ' '; i++)
        ;
    for (; command[i] != '\0'; i++, j++)
    {
        // WARNING: Not the best parsing of quotes
        if (command[i] == '"' || command[i] == '\'')
        {
            in_quotes = !in_quotes;
            if (after_space)
            {
                k++;
                j = -1;
            }
            after_space = 0;
            continue;
        }
        else if (in_quotes)
        {
            parsed_command[k][j] = command[i];
            continue;
        }

        if (command[i] == ' ')
        {
            if (!after_space)
            {
                parsed_command[k][j] = '\0';
            }
            after_space = 1;
            continue;
        }
        else
        {
            if (after_space)
            {
                k++;
                j = 0;
            }
            after_space = 0;
        }
        parsed_command[k][j] = command[i];
    }
    if (!after_space)
    {
        parsed_command[k][j] = '\0';
    }
    parsed_command[k + 1][0] = '\0';
    return parsed_command;
}

int is_client_command_help(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_HELP);
}

int is_client_command_list(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_LIST);
}

int is_client_command_readF(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_READF);
}

int is_client_command_writeT(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_WRITET);
}

int is_client_command_upload(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_UPLOAD);
}

int is_client_command_download(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_DOWNLOAD);
}

int is_client_command_archServer(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_ARCHSERVER);
}

int is_client_command_killServer(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_KILLSERVER);
}

int is_client_command_quit(const char **parsed_command)
{
    return are_strings_the_same(parsed_command[0], CLIENT_COMMAND_STRING_QUIT);
}

int write_without_interrupt(int fd, const void *buffer, size_t size)
{
    int bytes_written = 0;
    while ((bytes_written = write(fd, buffer, size)) == -1 && errno == EINTR)
        ;
    return bytes_written;
}

void get_decline_reason_response(int decline_reason, char *buffer)
{
    switch (decline_reason)
    {
    case DECLINE_REASON_CAPACITY:
        strcpy(buffer, SERVER_TO_CLIENT_RESPONSE_KILL_BY_CAPACITY);
        break;
    case DECLINE_REASON_SERVER_TERMINATED:
        strcpy(buffer, SERVER_TO_CLIENT_RESPONSE_KILL_BY_SERVER_TERMINATED);
        break;
    default:
        strcpy(buffer, INVALID_DECLINE_REASON);
        break;
    }
}

int is_decline_reason_string_valid(const char *decline_reason)
{
    return strcmp(decline_reason, INVALID_DECLINE_REASON) != 0;
}

int does_fifo_exist(const char *fifo_path)
{
    struct stat fifo_stat;
    if (stat(fifo_path, &fifo_stat) == 0 && S_ISFIFO(fifo_stat.st_mode))
    {
        return 1;
    }
    return 0;
}

int is_response_kill_by_server_terminated(const char *buffer)
{
    return strncmp(buffer, SERVER_TO_CLIENT_RESPONSE_KILL_BY_SERVER_TERMINATED, strlen(SERVER_TO_CLIENT_RESPONSE_KILL_BY_SERVER_TERMINATED)) == 0;
}

int is_response_kill_by_capacity(const char *buffer)
{
    return strncmp(buffer, SERVER_TO_CLIENT_RESPONSE_KILL_BY_CAPACITY, strlen(SERVER_TO_CLIENT_RESPONSE_KILL_BY_CAPACITY)) == 0;
}

int is_response_connection_accepted(const char *buffer)
{
    return strncmp(buffer, SERVER_TO_CLIENT_RESPONSE_CONNECTION_ACCEPTED, strlen(SERVER_TO_CLIENT_RESPONSE_CONNECTION_ACCEPTED)) == 0;
}

char *get_server_fifo_mutex_path(int server_pid, char *buffer)
{
    sprintf(buffer, "%s%d", CONNECTION_REQUEST_MUTEX_PATH_PREFIX, server_pid);
    return buffer;
}

char *get_server_data_mutex_path(int server_pid, char *buffer)
{
    sprintf(buffer, "%s%d", ARCHIVE_MUTEX_PATH_PREFIX, server_pid);
    return buffer;
}

char *get_file_data_mutex_path(const char *file_path, char *buffer)
{
    sprintf(buffer, "%s%s", FILE_DATA_MUTEX_PATH_PREFIX, get_filename(file_path));
    return buffer;
}

char *get_server_fifo_path(int server_pid, char *buffer)
{
    sprintf(buffer, "%s%d", SERVER_FIFO_PATH_PREFIX, server_pid);
    return buffer;
}

char *get_client_to_server_fifo_path(int client_pid, char *buffer)
{
    sprintf(buffer, "%s%d", CLIENT_TO_SERVER_FIFO_PATH_PREFIX, client_pid);
    return buffer;
}

char *get_server_to_client_fifo_path(int client_pid, char *buffer)
{
    sprintf(buffer, "%s%d", SERVER_TO_CLIENT_FIFO_PATH_PREFIX, client_pid);
    return buffer;
}

char *get_client_to_server_data_fifo_path(const char *file_path, char *buffer)
{
    sprintf(buffer, "%s%s", CLIENT_TO_SERVER_DATA_FIFO_PATH_PREFIX, get_filename(file_path));
    return buffer;
}

char *get_server_to_client_data_fifo_path(const char *file_path, char *buffer)
{
    sprintf(buffer, "%s%s", SERVER_TO_CLIENT_DATA_FIFO_PATH_PREFIX, get_filename(file_path));
    return buffer;
}

char *produce_blocking_connection_request(int client_pid, char *buffer)
{
    sprintf(buffer, "%s%s%d", CLIENT_CONNECTION_REQUEST_PREFIX, CLIENT_CONNECTION_REQUEST_BLOCK, client_pid);
    return buffer;
}

char *produce_nonblocking_connection_request(int client_pid, char *buffer)
{
    sprintf(buffer, "%s%s%d", CLIENT_CONNECTION_REQUEST_PREFIX, CLIENT_CONNECTION_REQUEST_NONBLOCK, client_pid);
    return buffer;
}

char *produce_connection_request(int client_pid, char *buffer, int nonblock)
{
    return nonblock ? produce_nonblocking_connection_request(client_pid, buffer) : produce_blocking_connection_request(client_pid, buffer);
}

int is_connection_request(const char *buffer)
{
    return strncmp(buffer, CLIENT_CONNECTION_REQUEST_PREFIX, strlen(CLIENT_CONNECTION_REQUEST_PREFIX)) == 0;
}

int is_connection_request_blocking(const char *buffer)
{
    return strncmp(buffer + strlen(CLIENT_CONNECTION_REQUEST_PREFIX), CLIENT_CONNECTION_REQUEST_BLOCK, strlen(CLIENT_CONNECTION_REQUEST_BLOCK)) == 0;
}

int is_connection_request_nonblocking(const char *buffer)
{
    return strncmp(buffer + strlen(CLIENT_CONNECTION_REQUEST_PREFIX), CLIENT_CONNECTION_REQUEST_NONBLOCK, strlen(CLIENT_CONNECTION_REQUEST_NONBLOCK)) == 0;
}

int get_connection_request_client_pid(const char *buffer)
{
    return atoi(buffer + strlen(CLIENT_CONNECTION_REQUEST_PREFIX) + CLIENT_CONNECTION_REQUEST_TYPE_FLAG_LENGTH);
}

#endif // SYSTEM_MIDTERM_SHARED_H
