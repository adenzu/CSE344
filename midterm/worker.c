#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <semaphore.h>

#include "shared.h"

/*--For Logging--*/

#define LOGS_DIRECTORY_RELATIVE_PATH "logs"
#define MAX_TIMESTAMP_LENGTH 64

char timestamp[MAX_TIMESTAMP_LENGTH] = "\0";

char *get_timestamp()
{
    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);
    strftime(timestamp, MAX_TIMESTAMP_LENGTH, "[%Y-%m-%d %H:%M:%S]", time_info);
    return timestamp;
}

char *get_timestamp_for_filename()
{
    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);
    strftime(timestamp, MAX_TIMESTAMP_LENGTH, "%Y_%m_%d_%H_%M_%S", time_info);
    return timestamp;
}

/*--For Logging--*/

char *server_directory_path = NULL;

int server_directory_fd;
int logs_directory_fd;
int log_file_fd;

int server_to_client_fifo_fd;
int client_to_server_fifo_fd;

void cleanup();
int log_raw(const char *);

void safe_exit()
{
    log_raw(SERVER_LOG_CONNECTION_END);
    cleanup();
    exit(EXIT_SUCCESS);
}

int is_client_alive(int client_pid)
{
    char server_to_client_fifo_path[MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH] = "\0";
    char client_to_server_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH] = "\0";
    return does_fifo_exist(get_server_to_client_fifo_path(client_pid, server_to_client_fifo_path)) &&
           does_fifo_exist(get_client_to_server_fifo_path(client_pid, client_to_server_fifo_path));
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

int open_client_to_server_fifo(int client_pid)
{
    char client_to_server_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH] = "\0";
    int client_to_server_fifo_fd = open(get_client_to_server_fifo_path(client_pid, client_to_server_fifo_path), O_RDONLY);
    if (client_to_server_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return client_to_server_fifo_fd;
}

int log_with_prefix(const char *string, const char *prefix)
{
    // Horrible writing with multiple calls instead of one united, but do not want to
    // fiddle with concatenation
    get_timestamp();
    if (write_without_interrupt(log_file_fd, timestamp, strlen(timestamp) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    if (write_without_interrupt(log_file_fd, prefix, strlen(prefix) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    if (write_without_interrupt(log_file_fd, string, strlen(string) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    if (write_without_interrupt(log_file_fd, "\n", strlen("\n") * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}

int log_raw(const char *string)
{
    return log_with_prefix(string, " ");
}

int log_response(const char *response)
{
    return log_with_prefix(response, " SERVER >>> ");
}

int log_command(const char *command)
{
    return log_with_prefix(command, " CLIENT >>> ");
}

int send_response_to_client_without_logging(const char *response)
{
    if (write_without_interrupt(server_to_client_fifo_fd, response, (strlen(response) + 1) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}

int send_response_to_client(const char *response)
{
    if (write_without_interrupt(server_to_client_fifo_fd, response, (strlen(response) + 1) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    log_response(response);
    return 0;
}

int send_unknown_command_response()
{
    char response[] = "Uknown command: use 'help' to see available commands";
    if (send_response_to_client(response) == -1)
    {
        return -1;
    }
    return 0;
}

int send_missing_arguments_response()
{
    char response[] = "Command is missing required arguments";
    if (send_response_to_client(response) == -1)
    {
        return -1;
    }
    return 0;
}

int send_invalid_arguments_response()
{
    char response[] = "Command contains invalid arguments";
    if (send_response_to_client(response) == -1)
    {
        return -1;
    }
    return 0;
}

int upload_file(const char *file_path)
{
    char server_to_client_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    get_server_to_client_data_fifo_path(file_path, server_to_client_data_fifo_path);

    // NOTE: Should upload with different name if file with the same name is already being uploaded?
    if (does_fifo_exist(server_to_client_data_fifo_path))
    {
        return 0;
    }

    int fd = openat(server_directory_fd, file_path, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return -1;
    }

    if (mkfifo(server_to_client_data_fifo_path, 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }

    int server_to_client_data_fifo_fd = open(server_to_client_data_fifo_path, O_WRONLY);
    if (server_to_client_data_fifo_fd == -1)
    {
        perror("open");
        unlink(server_to_client_data_fifo_path);
        return -1;
    }

    int read_bytes;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(server_to_client_data_fifo_fd, buffer, read_bytes) == -1)
        {
            perror("write");
            close(server_to_client_data_fifo_fd);
            unlink(server_to_client_data_fifo_path);
            close(fd);
            return -1;
        }
    }

    if (read_bytes == -1)
    {
        perror("read");
        close(server_to_client_data_fifo_fd);
        unlink(server_to_client_data_fifo_path);
        close(fd);
        return -1;
    }

    close(server_to_client_data_fifo_fd);
    close(fd);
    return 0;
}

int download_file(const char *file_path)
{
    char valid_file_name[MAX_FILE_NAME_LENGTH];
    if (find_valid_name_relative(server_directory_fd, file_path, valid_file_name) == -1)
    {
        send_response_to_client("Server has too many files with the same name");
        return -1;
    }

    char archive_mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *archive_sem = sem_open(get_server_data_mutex_path(getppid(), archive_mutex_path), O_CREAT, 0666, 1);
    sem_wait(archive_sem);

    char mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *sem = sem_open(get_file_data_mutex_path(file_path, mutex_path), O_CREAT, 0666, 1);
    sem_wait(sem);

    char client_to_server_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    get_client_to_server_data_fifo_path(file_path, client_to_server_data_fifo_path);

    int client_to_server_data_fifo_fd = open(client_to_server_data_fifo_path, O_RDONLY);
    if (client_to_server_data_fifo_fd == -1)
    {
        perror("open");
        unlink(client_to_server_data_fifo_path);
        sem_post(sem);
        sem_close(sem);
        sem_unlink(mutex_path);
        sem_post(archive_sem);
        sem_close(archive_sem);
        sem_unlink(archive_mutex_path);
        return -1;
    }

    int file_fd = openat(server_directory_fd, valid_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd == -1)
    {
        perror("openat");
        close(client_to_server_data_fifo_fd);
        unlink(client_to_server_data_fifo_path);
        sem_post(sem);
        sem_close(sem);
        sem_unlink(mutex_path);
        sem_post(archive_sem);
        sem_close(archive_sem);
        sem_unlink(archive_mutex_path);
        return -1;
    }

    int read_bytes;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while ((read_bytes = read(client_to_server_data_fifo_fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(file_fd, buffer, read_bytes) == -1)
        {
            perror("write");
            close(file_fd);
            close(client_to_server_data_fifo_fd);
            unlink(client_to_server_data_fifo_path);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            sem_post(archive_sem);
            sem_close(archive_sem);
            sem_unlink(archive_mutex_path);
            return -1;
        }
    }

    if (read_bytes == -1)
    {
        perror("read");
        close(file_fd);
        close(client_to_server_data_fifo_fd);
        unlink(client_to_server_data_fifo_path);
        sem_post(sem);
        sem_close(sem);
        sem_unlink(mutex_path);
        sem_post(archive_sem);
        sem_close(archive_sem);
        sem_unlink(archive_mutex_path);
        return -1;
    }

    close(file_fd);
    close(client_to_server_data_fifo_fd);
    unlink(client_to_server_data_fifo_path);
    sem_post(sem);
    sem_close(sem);
    sem_unlink(mutex_path);
    sem_post(archive_sem);
    sem_close(archive_sem);
    sem_unlink(archive_mutex_path);
    return 0;
}

int hande_help_command(const char **parsed_command)
{
    if (is_client_command_help(&parsed_command[1]))
    {
        char response[] = "help <command>\n\tDisplay the list of possible client requests";
        send_response_to_client(response);
    }
    else if (is_client_command_list(&parsed_command[1]))
    {
        char response[] = "list\n\tSends a request to display the list of files in the Servers directory (also displays the list received from the Server)";
        send_response_to_client(response);
    }
    else if (is_client_command_readF(&parsed_command[1]))
    {
        char response[] = "readF <file> <line #>\n\tRequests to display the # line of the <file>. If no line number is given, the whole contents of the file is requested (and displayed on the client side)";
        send_response_to_client(response);
    }
    else if (is_client_command_writeT(&parsed_command[1]))
    {
        char response[] = "writeT <file> <line #> <string>\n\tRequests to write the content of \"string\" to the #th line of the <file>. If the line # is not given, writes to the end of the file. If the file does not exist in the Servers directory, it creates and edits the file at the same time";
        send_response_to_client(response);
    }
    else if (is_client_command_upload(&parsed_command[1]))
    {
        char response[] = "upload <file>\n\tUploads the file from the current working directory of the client to the Servers directory (beware of cases where there is no file in the client's current working directory and a file with the same name on the Servers side)";
        send_response_to_client(response);
    }
    else if (is_client_command_download(&parsed_command[1]))
    {
        char response[] = "download <file>\n\tRequests to receive <file> from the Servers directory to the client side";
        send_response_to_client(response);
    }
    else if (is_client_command_archServer(&parsed_command[1]))
    {
        char response[] = "archServer <fileName>.tar\n\tUsing fork, exec, and tar utilities, creates a child process that will collect all the files currently available on the Server side and store them in the <filename>.tar archive";
        send_response_to_client(response);
    }
    else if (is_client_command_killServer(&parsed_command[1]))
    {
        char response[] = "killServer\n\tSends a kill request to the Server";
        send_response_to_client(response);
    }
    else if (is_client_command_quit(&parsed_command[1]))
    {
        char response[] = "quit\n\tSends a write request to the Server-side log file and quits";
        send_response_to_client(response);
    }
    else if (parsed_command[1][0] == '\0')
    {
        char response[] = "help, list, readF,  writeT, upload, download, archServer, killServer, quit";
        send_response_to_client(response);
    }
    else
    {
        send_unknown_command_response();
    }
    return 0;
}

int handle_list_command(const char **parsed_command)
{
    DIR *dir = opendir(server_directory_path);
    if (dir == NULL)
    {
        perror("fdopendir");
        return -1;
    }

    char response[MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH] = "\0";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            continue;
        }
        if (strlen(response) + strlen(entry->d_name) + 1 >= MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH - 1)
        {
            send_response_to_client(response);
            response[0] = '\0';
        }

        strcat(response, entry->d_name);
        strcat(response, "\n");
    }

    closedir(dir);

    send_response_to_client(response);

    return 0;
}

int handle_readF_command(const char **parsed_command)
{
    const char *file_path = parsed_command[1];
    const char *line_index_string = parsed_command[2];

    if (file_path[0] == '\0')
    {
        send_missing_arguments_response();
        return 0;
    }

    char mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *sem = sem_open(get_file_data_mutex_path(file_path, mutex_path), O_CREAT, 0666, 1);
    sem_wait(sem);

    int fd = openat(server_directory_fd, file_path, O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            send_invalid_arguments_response();
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            return 0;
        }
        perror("openat");
        sem_post(sem);
        sem_close(sem);
        sem_unlink(mutex_path);
        return -1;
    }

    if (line_index_string[0] == '\0')
    {
        int read_bytes;
        char buffer[MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH];
        while ((read_bytes = read(fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[read_bytes] = '\0';
            send_response_to_client(buffer);
        }

        if (read_bytes == -1)
        {
            perror("read");
            close(fd);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            return -1;
        }
    }
    else
    {
        int line_index = atoi(line_index_string);
        if (line_index < 0)
        {
            send_invalid_arguments_response();
            close(fd);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            return 0;
        }

        int i;
        int read_bytes;
        char response[MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH] = "\0";
        char buffer[MAX_SERVER_TO_CLIENT_RESPONSE_LENGTH];
        while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0)
        {
            int line_offset = 0;
            int next_line_offset = 0;
            for (i = 0; i < read_bytes && 0 < (line_index + 1); i++)
            {
                if (buffer[i] == '\n')
                {
                    line_index--;
                    line_offset = next_line_offset;
                    next_line_offset = i + 1;
                }
            }
            if (i <= read_bytes)
            {
                strncat(response, &buffer[line_offset], next_line_offset - line_offset);
                response[strlen(response) - 1] = '\0';
                break;
            }
            else if (line_index == 0)
            {
                printf("line_offset: %d\n", line_offset);
                strncat(response, &buffer[next_line_offset], read_bytes - next_line_offset);
            }
        }

        if (read_bytes == -1)
        {
            perror("read");
            close(fd);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            return -1;
        }

        if (line_index == -1)
        {
            send_response_to_client(response);
        }
        else
        {
            send_response_to_client("Invalid argument: given line index is too high");
        }
    }

    close(fd);
    sem_post(sem);
    sem_close(sem);
    sem_unlink(mutex_path);
    return 0;
}

int handle_writeT_command(const char **parsed_command)
{
    const char *file_path = parsed_command[1];
    const char *to_write = parsed_command[2];
    const char *line_index_string = parsed_command[3];

    if (file_path[0] == '\0' || to_write[0] == '\0')
    {
        send_missing_arguments_response();
        return 0;
    }

    char archive_mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *archive_sem = sem_open(get_server_data_mutex_path(getppid(), archive_mutex_path), O_CREAT, 0666, 1);
    sem_wait(archive_sem);

    char mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *sem = sem_open(get_file_data_mutex_path(file_path, mutex_path), O_CREAT, 0666, 1);
    sem_wait(sem);

    if (line_index_string[0] == '\0')
    {
        int fd = openat(server_directory_fd, file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1)
        {
            perror("openat");
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            sem_post(archive_sem);
            sem_close(archive_sem);
            sem_unlink(archive_mutex_path);
            return -1;
        }
        if (write(fd, to_write, strlen(to_write) * sizeof(char)) == -1)
        {
            perror("write");
            close(fd);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            sem_post(archive_sem);
            sem_close(archive_sem);
            sem_unlink(archive_mutex_path);
            return -1;
        }
        if (write(fd, "\n", sizeof(char)) == -1)
        {
            perror("write");
            close(fd);
            sem_post(sem);
            sem_close(sem);
            sem_unlink(mutex_path);
            sem_post(archive_sem);
            sem_close(archive_sem);
            sem_unlink(archive_mutex_path);
            return -1;
        }
        close(fd);
    }
    else
    {
        int fd = openat(server_directory_fd, file_path, O_RDWR);
        if (fd == -1)
        {
            if (errno != ENOENT)
            {
                perror("openat");
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
            fd = openat(server_directory_fd, file_path, O_WRONLY | O_CREAT, 0666);
            if (fd == -1)
            {
                perror("openat");
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
            if (write(fd, to_write, strlen(to_write) * sizeof(char)) == -1)
            {
                perror("write");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
            if (write(fd, "\n", sizeof(char)) == -1)
            {
                perror("write");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
        }
        else
        {
            int line_index = atoi(line_index_string);
            if (line_index < 0)
            {
                send_invalid_arguments_response();
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return 0;
            }

            const int max_line_length = 1024;

            int line_offset = 0;
            int read_bytes;
            char buffer[max_line_length];
            while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0)
            {
                for (int i = 0; i < read_bytes && 0 < line_index; i++)
                {
                    if (buffer[i] == '\n')
                    {
                        line_index--;
                        line_offset = i + 1;
                    }
                }
            }

            if (read_bytes == -1)
            {
                perror("read");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }

            if (line_index != 0)
            {
                send_response_to_client("Invalid argument: given line index is too high");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return 0;
            }

            lseek(fd, line_offset, SEEK_SET);

            if (write(fd, to_write, strlen(to_write) * sizeof(char)) == -1)
            {
                perror("write");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
            if (write(fd, "\n", sizeof(char)) == -1)
            {
                perror("write");
                close(fd);
                sem_post(sem);
                sem_close(sem);
                sem_unlink(mutex_path);
                sem_post(archive_sem);
                sem_close(archive_sem);
                sem_unlink(archive_mutex_path);
                return -1;
            }
        }
        close(fd);
    }
    sem_post(sem);
    sem_close(sem);
    sem_unlink(mutex_path);
    sem_post(archive_sem);
    sem_close(archive_sem);
    sem_unlink(archive_mutex_path);
    send_response_to_client("File successfully written");
    return 0;
}

int handle_upload_command(const char **parsed_command)
{
    const char *file_path = parsed_command[1];

    if (file_path[0] == '\0')
    {
        send_missing_arguments_response();
        return 0;
    }

    if (download_file(file_path) == -1)
    {
        send_response_to_client("Failed to receive file");
        return -1;
    }

    send_response_to_client("File successfully received");
    return 0;
}

int handle_download_command(const char **parsed_command)
{
    const char *file_path = parsed_command[1];

    if (file_path[0] == '\0')
    {
        send_missing_arguments_response();
        return 0;
    }

    if (!check_file_exists_relative(server_directory_fd, file_path))
    {
        send_response_to_client("File does not exist");
        return 0;
    }

    if (upload_file(file_path) == -1)
    {
        send_response_to_client("Failed to send file");
        return -1;
    }

    send_response_to_client("File successfully sent");
    return 0;
}

int handle_archServer_command(const char **parsed_command)
{
    char archive_mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *archive_sem = sem_open(get_server_data_mutex_path(getppid(), archive_mutex_path), O_CREAT, 0666, 1);
    sem_wait(archive_sem);

    int pid = fork();
    if (pid < 0)
    {
        perror("fork");
        sem_post(archive_sem);
        sem_close(archive_sem);
        sem_unlink(archive_mutex_path);
        return -1;
    }
    else if (pid == 0)
    {
        char tar_filename[MAX_FILE_NAME_LENGTH];
        snprintf(tar_filename, sizeof(tar_filename), "archive_%s.tar", get_timestamp_for_filename());

        int tar_fd = openat(server_directory_fd, tar_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (tar_fd == -1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(tar_fd, STDOUT_FILENO);
        close(tar_fd);

        execlp("tar", "tar", "cf", "-", "--exclude", "archive_*.tar", server_directory_path, NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            send_response_to_client("Archive created successfully");
        }
        else
        {
            send_response_to_client("Failed to create archive");
        }
    }
    sem_post(archive_sem);
    sem_close(archive_sem);
    sem_unlink(archive_mutex_path);
    return 0;
}

int handle_killServer_command(const char **parsed_command)
{
    char archive_mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *archive_sem = sem_open(get_server_data_mutex_path(getppid(), archive_mutex_path), O_CREAT, 0666, 1);
    sem_wait(archive_sem);

    kill(getppid(), SIGINT);

    sem_post(archive_sem);
    sem_close(archive_sem);
    sem_unlink(archive_mutex_path);
    return 0;
}

int handle_client_command(const char *command, int buffer_length)
{
    log_command(command);

    char **parsed_command = parse_client_command(command, allocate_command_array());
    const char **const_parsed_command = (const char **)parsed_command;

    if (is_client_command_help(const_parsed_command))
    {
        hande_help_command(const_parsed_command);
    }
    else if (is_client_command_list(const_parsed_command))
    {
        handle_list_command(const_parsed_command);
    }
    else if (is_client_command_readF(const_parsed_command))
    {
        handle_readF_command(const_parsed_command);
    }
    else if (is_client_command_writeT(const_parsed_command))
    {
        handle_writeT_command(const_parsed_command);
    }
    else if (is_client_command_upload(const_parsed_command))
    {
        handle_upload_command(const_parsed_command);
    }
    else if (is_client_command_download(const_parsed_command))
    {
        handle_download_command(const_parsed_command);
    }
    else if (is_client_command_archServer(const_parsed_command))
    {
        handle_archServer_command(const_parsed_command);
    }
    else if (is_client_command_killServer(const_parsed_command))
    {
        handle_killServer_command(const_parsed_command);
    }
    else if (is_client_command_quit(const_parsed_command))
    {
        free_command_array(parsed_command);
        safe_exit();
    }
    else
    {
        send_unknown_command_response();
    }

    free_command_array(parsed_command);
    return 0;
}

int handle_client_commands(const char *buffer, int buffer_length)
{
    int offset = 0;
    for (int i = 0; i < buffer_length; i++)
    {
        if (buffer[i] == '\0')
        {
            if (handle_client_command(buffer + offset, i - offset) == -1)
            {
                return -1;
            }
            offset = i + 1;
        }
    }
    return 0;
}

int work(int client_pid)
{
    if ((server_to_client_fifo_fd = open_server_to_client_fifo(client_pid)) == -1)
    {
        return 1;
    }

    if (send_response_to_client_without_logging(SERVER_TO_CLIENT_RESPONSE_CONNECTION_ACCEPTED) == -1)
    {
        return 1;
    }

    if ((client_to_server_fifo_fd = open_client_to_server_fifo(client_pid)) == -1)
    {
        return 1;
    }

    log_raw(SERVER_LOG_CONNECTION_START);

    int read_bytes;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while (1)
    {
        read_bytes = read(client_to_server_fifo_fd, buffer, sizeof(buffer));

        if (read_bytes == -1)
        {
            perror("read");
            return -1;
        }

        if (read_bytes == 0)
        {
            // EOF
            break;
        }

        if (handle_client_commands(buffer, read_bytes) == -1)
        {
            return -1;
        }
    }

    return 0;
}

int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        perror("stat");
        return -1;
    }
    return S_ISDIR(st.st_mode);
}

int open_log_file(int client_pid)
{
    char log_file_path[MAX_LOG_FILE_PATH_LENGTH];
    sprintf(log_file_path, "client%d.log", client_pid);
    log_file_fd = openat(logs_directory_fd, log_file_path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (log_file_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int close_log_file()
{
    if (close(log_file_fd) == -1)
    {
        if (errno != EBADF)
        {
            perror("close");
            return -1;
        }
    }
    return 0;
}

int open_server_directory(const char *path)
{
    server_directory_fd = open(path, O_RDONLY);
    if (server_directory_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int close_server_directory()
{
    if (close(server_directory_fd) == -1)
    {
        if (errno != EBADF)
        {
            perror("close");
            return -1;
        }
    }
    return 0;
}

int open_logs_directory()
{
    logs_directory_fd = openat(server_directory_fd, LOGS_DIRECTORY_RELATIVE_PATH, O_RDONLY);
    if (logs_directory_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int close_logs_directory()
{
    if (close(logs_directory_fd) == -1)
    {
        if (errno != EBADF)
        {
            perror("close");
            return -1;
        }
    }
    return 0;
}

int close_server_to_client_fifo()
{
    if (close(server_to_client_fifo_fd) == -1)
    {
        if (errno != EBADF)
        {
            perror("close");
            return -1;
        }
    }
    return 0;
}

int close_client_to_server_fifo()
{
    if (close(client_to_server_fifo_fd) == -1)
    {
        if (errno != EBADF)
        {
            perror("close");
            return -1;
        }
    }
    return 0;
}

void cleanup()
{
    close_server_to_client_fifo();
    close_client_to_server_fifo();
    close_log_file();
    close_logs_directory();
    close_server_directory();
}

void sigint_handler(int signum)
{
    safe_exit();
}

int connect_sigint_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int prepare_logs_directory()
{
    struct stat st;
    if (fstatat(server_directory_fd, LOGS_DIRECTORY_RELATIVE_PATH, &st, 0) == -1)
    {
        if (errno == ENOENT)
        {
            if (mkdirat(server_directory_fd, LOGS_DIRECTORY_RELATIVE_PATH, 0777) == -1)
            {
                perror("mkdirat");
                return -1;
            }
        }
        else
        {
            perror("fstatat");
            return -1;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        printf("Path %s is not a directory\n", LOGS_DIRECTORY_RELATIVE_PATH);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <client_pid> <server_directory_path>\n", argv[0]);
        return 1;
    }

    char *client_id_string = argv[1];
    char *server_directory_path_arg = argv[2];

    if (is_directory(server_directory_path_arg) != 1)
    {
        printf("Invalid server directory path\n");
        return 1;
    }

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

    if (connect_sigint_handler() == -1)
    {
        return 1;
    }

    if (open_server_directory(server_directory_path_arg) == -1)
    {
        return 1;
    }

    server_directory_path = server_directory_path_arg;

    if (prepare_logs_directory() == -1)
    {
        return 1;
    }

    if (open_logs_directory() == -1)
    {
        return 1;
    }

    if (open_log_file(client_pid) == -1)
    {
        return 1;
    }

    if (work(client_pid) == -1)
    {
        return 1;
    }

    safe_exit();

    return 0;
}
