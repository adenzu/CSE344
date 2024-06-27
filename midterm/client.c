#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>

#include "shared.h"

// TODO: Does not exit sometimes when ctrl+c is pressed

/*----------COMMON----------*/

#define AGREED_TERMINATION_SIGNAL SIGUSR1
#define AGREED_SENDER_WORKER_START_SIGNAL SIGUSR2

int living_children = 0;

/*----------COMMON----------*/

/*----------WORKERS----------*/

#define FILE_FIFO_COUNTER_DIGITS 3

char client_to_server_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH] = "\0";
char server_to_client_fifo_path[MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH] = "\0";

int client_to_server_fifo_fd;
int server_to_client_fifo_fd;

char *get_this_client_to_server_fifo_path()
{
    if (client_to_server_fifo_path[0] == '\0')
    {
        get_client_to_server_fifo_path(getppid(), client_to_server_fifo_path);
    }
    return client_to_server_fifo_path;
}

char *get_this_server_to_client_fifo_path()
{
    if (server_to_client_fifo_path[0] == '\0')
    {
        get_server_to_client_fifo_path(getppid(), server_to_client_fifo_path);
    }
    return server_to_client_fifo_path;
}

int create_client_to_server_fifo()
{
    if (mkfifo(get_this_client_to_server_fifo_path(), 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }
    return 0;
}

int create_server_to_client_fifo()
{
    if (mkfifo(get_this_server_to_client_fifo_path(), 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }
    return 0;
}

int create_client_to_server_data_fifo(const char *file_path)
{
    char client_to_server_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    if (mkfifo(get_client_to_server_data_fifo_path(file_path, client_to_server_data_fifo_path), 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }
    return 0;
}

int open_client_to_server_data_fifo_for_writing(const char *file_path)
{
    char client_to_server_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    int client_to_server_data_fifo_fd = open(get_client_to_server_data_fifo_path(file_path, client_to_server_data_fifo_path), O_WRONLY);
    if (client_to_server_data_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return client_to_server_data_fifo_fd;
}

int open_server_to_client_data_fifo_for_reading(const char *file_name)
{
    char server_to_client_data_fifo_path[MAX_SERVER_TO_CLIENT_FIFO_PATH_LENGTH];
    int server_to_client_data_fifo_fd = open(get_server_to_client_data_fifo_path(file_name, server_to_client_data_fifo_path), O_RDONLY);
    if (server_to_client_data_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return server_to_client_data_fifo_fd;
}

int unlink_client_to_server_fifo()
{
    if (unlink(get_this_client_to_server_fifo_path()) == -1)
    {
        if (errno == ENOENT)
        {
            // The file does not exist.
            // This is not treated as an error.
            return 0;
        }
        perror("unlink");
        return -1;
    }
    return 0;
}

int unlink_server_to_client_fifo()
{
    if (unlink(get_this_server_to_client_fifo_path()) == -1)
    {
        if (errno == ENOENT)
        {
            // The file does not exist.
            // This is not treated as an error.
            return 0;
        }
        perror("unlink");
        return -1;
    }
    return 0;
}

int create_fifos()
{
    if (create_client_to_server_fifo() == -1)
    {
        return -1;
    }
    if (create_server_to_client_fifo() == -1)
    {
        unlink_client_to_server_fifo();
        return -1;
    }
    return 0;
}

int unlink_fifos()
{
    if (unlink_client_to_server_fifo() == -1)
    {
        return -1;
    }
    if (unlink_server_to_client_fifo() == -1)
    {
        return -1;
    }
    return 0;
}

int open_client_to_server_fifo_for_writing()
{
    client_to_server_fifo_fd = open(get_this_client_to_server_fifo_path(), O_WRONLY);
    if (client_to_server_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int close_client_to_server_fifo()
{
    if (close(client_to_server_fifo_fd) == -1)
    {
        // WARNING: errno
        if (errno == EBADF)
        {
            // The file descriptor is not valid.
            // This is not treated as an error.
            return 0;
        }
        perror("close");
        return -1;
    }
    return 0;
}

int open_server_to_client_fifo_for_reading()
{
    server_to_client_fifo_fd = open(get_this_server_to_client_fifo_path(), O_RDONLY);
    if (server_to_client_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int close_server_to_client_fifo()
{
    if (close(server_to_client_fifo_fd) == -1)
    {
        // WARNING: errno
        if (errno == EBADF)
        {
            // The file descriptor is not valid.
            // This is not treated as an error.
            return 0;
        }
        perror("close");
        return -1;
    }
    return 0;
}

int open_fifos()
{
    if (open_client_to_server_fifo_for_writing() == -1)
    {
        return -1;
    }
    if (open_server_to_client_fifo_for_reading() == -1)
    {
        return -1;
    }
    return 0;
}

int close_fifos()
{
    if (close_client_to_server_fifo() == -1)
    {
        return -1;
    }
    if (close_server_to_client_fifo() == -1)
    {
        return -1;
    }
    return 0;
}

int send_termination_signal_to_parent()
{
    if (kill(getppid(), AGREED_TERMINATION_SIGNAL) == -1)
    {
        perror("kill");
        return -1;
    }
    return 0;
}

int send_start_signal_to_parent()
{
    if (kill(getppid(), AGREED_SENDER_WORKER_START_SIGNAL) == -1)
    {
        perror("kill");
        return -1;
    }
    return 0;
}

int handle_server_response_part(const char *part, int part_length)
{
    if (is_response_kill_by_capacity(part))
    {
        printf("Server is full. Exiting.\n");
        if (send_termination_signal_to_parent() == -1)
        {
            return -1;
        }
        return 0;
    }
    else if (is_response_kill_by_server_terminated(part))
    {
        printf("Server terminated. Exiting.\n");
        if (send_termination_signal_to_parent() == -1)
        {
            return -1;
        }
        return 0;
    }
    else if (is_response_connection_accepted(part))
    {
        printf("Connected to the server.\n");
        send_start_signal_to_parent();
        return 0;
    }
    else
    {
        write(STDOUT_FILENO, "SERVER >>> ", 11);
        write(STDOUT_FILENO, part, part_length);
        write(STDOUT_FILENO, "\n", 1);
    }

    return 0;
}

int handle_server_response(const char *buffer, int buffer_size)
{
    int offset = 0;
    for (int i = 0; i < buffer_size; i++)
    {
        if (buffer[i] == '\0')
        {
            if (handle_server_response_part(buffer + offset, i - offset) == -1)
            {
                return -1;
            }
            offset = i + 1;
        }
    }
    return 0;
}

int receiver_worker_cleanup()
{
    if (close_server_to_client_fifo() == -1)
    {
        return -1;
    }
    if (unlink_server_to_client_fifo() == -1)
    {
        return -1;
    }
    return 0;
}

void receiver_worker_termination_signal_handler(int signum)
{
    receiver_worker_cleanup();
    printf("Client receiver worker exiting...\n");
    exit(EXIT_SUCCESS);
}

int connect_receiver_worker_termination_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = receiver_worker_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(AGREED_TERMINATION_SIGNAL, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_receiver_worker_sigint_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = receiver_worker_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int receiver_worker()
{
    if (connect_receiver_worker_termination_signal_handler() == -1)
    {
        return -1;
    }

    if (connect_receiver_worker_sigint_signal_handler() == -1)
    {
        return -1;
    }

    if (create_server_to_client_fifo() == -1)
    {
        return -1;
    }

    if (open_server_to_client_fifo_for_reading() == -1)
    {
        unlink_server_to_client_fifo();
        return -1;
    }

    int read_bytes = 0;
    char buffer[SERVER_TO_CLIENT_FIFO_BUFFER_SIZE];
    // WARNING: SIGINTR may cause read() to return -1 with errno set to EINTR.
    while ((read_bytes = read(server_to_client_fifo_fd, buffer, sizeof(buffer))) > 0)
    {
        handle_server_response(buffer, read_bytes);
    }

    printf("Server disconnected.\n");
    send_termination_signal_to_parent();

    if (read_bytes == -1)
    {
        perror("read");
        return -1;
    }

    if (close_server_to_client_fifo() == -1)
    {
        unlink_server_to_client_fifo();
        return -1;
    }

    if (unlink_server_to_client_fifo() == -1)
    {
        return -1;
    }

    return 0;
}

int sender_worker_cleanup()
{
    if (close_client_to_server_fifo() == -1)
    {
        return -1;
    }
    if (unlink_client_to_server_fifo() == -1)
    {
        return -1;
    }
    return 0;
}

#define SENDER_WORKER_SLEEP_INTERVAL 60 // seconds

int waiting_for_connection = 1;

void sender_worker_start_signal_handler(int signum)
{
    waiting_for_connection = 0;
}

void sender_worker_termination_signal_handler(int signum)
{
    sender_worker_cleanup();
    printf("Client sender worker exiting...\n");
    exit(EXIT_SUCCESS);
}

int connect_sender_worker_termination_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = sender_worker_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(AGREED_TERMINATION_SIGNAL, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_sender_worker_sigint_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = sender_worker_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_sender_worker_start_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = sender_worker_start_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(AGREED_SENDER_WORKER_START_SIGNAL, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int send_command_to_server(const char *command, int command_length)
{
    if (write_without_interrupt(client_to_server_fifo_fd, command, (command_length + 1) * sizeof(char)) == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}

int replace_newline_with_null(char *buffer, int buffer_length)
{
    for (int i = buffer_length - 1; i >= 0; i--)
    {
        if (buffer[i] == '\n')
        {
            buffer[i] = '\0';
            return i;
        }
    }
    return buffer_length;
}

int upload_file(const char *file_path)
{
    char client_to_server_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    get_client_to_server_data_fifo_path(file_path, client_to_server_data_fifo_path);

    // NOTE: Should upload with different name if file with the same name is already being uploaded?
    if (does_fifo_exist(client_to_server_data_fifo_path))
    {
        printf("File '%s' is already being uploaded.\n", file_path);
        return 0;
    }

    int fd = open(file_path, O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            printf("File '%s' does not exist.\n", file_path);
            return -1;
        }
        perror("open");
        return -1;
    }

    if (mkfifo(client_to_server_data_fifo_path, 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }

    int client_to_server_data_fifo_fd = open(client_to_server_data_fifo_path, O_WRONLY);
    if (client_to_server_data_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }

    printf("Uploading file '%s'...\n", file_path);

    int read_bytes;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(client_to_server_data_fifo_fd, buffer, read_bytes) == -1)
        {
            perror("write");
            close(client_to_server_data_fifo_fd);
            unlink(client_to_server_data_fifo_path);
            close(fd);
            return -1;
        }
    }

    if (read_bytes == -1)
    {
        perror("read");
        close(client_to_server_data_fifo_fd);
        unlink(client_to_server_data_fifo_path);
        close(fd);
        return -1;
    }

    printf("File '%s' uploaded.\n", file_path);

    close(client_to_server_data_fifo_fd);
    close(fd);
    return 0;
}

int download_file(const char *file_path)
{
    char valid_file_name[MAX_FILE_NAME_LENGTH];
    const char *file_name = get_filename(file_path);

    if (find_valid_name(file_path, valid_file_name) == -1)
    {
        printf("There are too many files with name '%s'.\n", file_name);
        return -1;
    }

    char server_to_client_data_fifo_path[MAX_CLIENT_TO_SERVER_FIFO_PATH_LENGTH];
    get_server_to_client_data_fifo_path(file_path, server_to_client_data_fifo_path);

    // NOTE: Bad practice
    int limit = 3;
    for (int i = 0; i < limit; i++)
    {
        if (!does_fifo_exist(server_to_client_data_fifo_path))
        {
            sleep(1);
        }
    }

    if (!does_fifo_exist(server_to_client_data_fifo_path))
    {
        printf("Failed to download file '%s'.\n", file_path);
        return -1;
    }

    int server_to_client_data_fifo_fd = open(server_to_client_data_fifo_path, O_RDONLY);
    if (server_to_client_data_fifo_fd == -1)
    {
        perror("open");
        unlink(server_to_client_data_fifo_path);
        return -1;
    }

    int file_fd = open(valid_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd == -1)
    {
        perror("openat");
        close(server_to_client_data_fifo_fd);
        unlink(server_to_client_data_fifo_path);
        return -1;
    }

    int read_bytes;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while ((read_bytes = read(server_to_client_data_fifo_fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(file_fd, buffer, read_bytes) == -1)
        {
            perror("write");
            close(file_fd);
            close(server_to_client_data_fifo_fd);
            unlink(server_to_client_data_fifo_path);
            return -1;
        }
    }

    if (read_bytes == -1)
    {
        perror("read");
        close(file_fd);
        close(server_to_client_data_fifo_fd);
        unlink(server_to_client_data_fifo_path);
        return -1;
    }

    close(file_fd);
    close(server_to_client_data_fifo_fd);
    unlink(server_to_client_data_fifo_path);

    printf("File '%s' downloaded as '%s'.\n", file_path, valid_file_name);
    return 0;
}

int client_side_command_handling(const char *command, int command_length)
{
    char **parsed_command = parse_client_command(command, allocate_command_array());
    if (is_client_command_upload((const char **)parsed_command))
    {
        const char *file_path = parsed_command[1];
        if (upload_file(file_path) == -1)
        {
            free_command_array(parsed_command);
            return -1;
        }
    }
    if (is_client_command_download((const char **)parsed_command))
    {
        const char *file_path = parsed_command[1];
        if (download_file(file_path) == -1)
        {
            free_command_array(parsed_command);
            return -1;
        }
    }
    free_command_array(parsed_command);
    return 0;
}

int sender_worker()
{
    if (connect_sender_worker_termination_signal_handler() == -1)
    {
        return -1;
    }

    if (connect_sender_worker_sigint_signal_handler() == -1)
    {
        return -1;
    }

    if (connect_sender_worker_start_signal_handler() == -1)
    {
        return -1;
    }

    if (create_client_to_server_fifo() == -1)
    {
        return -1;
    }

    if (open_client_to_server_fifo_for_writing() == -1)
    {
        unlink_client_to_server_fifo();
        return -1;
    }

    while (waiting_for_connection)
    {
        sleep(SENDER_WORKER_SLEEP_INTERVAL);
    }

    int read_bytes = 0;
    char buffer[CLIENT_TO_SERVER_FIFO_BUFFER_SIZE];
    while (1)
    {
        read_bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
        read_bytes = replace_newline_with_null(buffer, read_bytes);
        send_command_to_server(buffer, read_bytes);
        client_side_command_handling(buffer, read_bytes);
    }

    if (close_client_to_server_fifo() == -1)
    {
        unlink_client_to_server_fifo();
        return -1;
    }

    if (unlink_client_to_server_fifo() == -1)
    {
        return -1;
    }

    return 0;
}

/*----------WORKERS----------*/

#define PARENT_PROCESS_SLEEP_INTERVAL 5 // seconds

pid_t receiver_worker_pid;
pid_t sender_worker_pid;

int send_start_signal_to_sender_worker()
{
    if (kill(sender_worker_pid, AGREED_SENDER_WORKER_START_SIGNAL) == -1)
    {
        perror("kill");
        return -1;
    }
    return 0;
}

int send_termination_signal_to_worker(pid_t pid)
{
    if (kill(pid, AGREED_TERMINATION_SIGNAL) == -1)
    {
        // WARNING: errno
        if (errno == ESRCH)
        {
            // The process with the given PID does not exist.
            // This is not treated as an error.
            return 0;
        }
        perror("kill");
        return -1;
    }
    return 0;
}

int parent_cleanup()
{
    send_termination_signal_to_worker(receiver_worker_pid);
    send_termination_signal_to_worker(sender_worker_pid);
    waitpid(receiver_worker_pid, NULL, 0);
    waitpid(sender_worker_pid, NULL, 0);
    return 0;
}

void parent_sigchld_signal_handler(int signum)
{
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            living_children--;
        }
    }
}

void parent_start_signal_handler(int signum)
{
    send_start_signal_to_sender_worker();
}

void parent_termination_signal_handler(int signum)
{
    parent_cleanup();
    printf("Client exiting...\n");
    exit(EXIT_SUCCESS);
}

int connect_parent_start_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = parent_start_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(AGREED_SENDER_WORKER_START_SIGNAL, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_parent_termination_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = parent_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(AGREED_TERMINATION_SIGNAL, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_parent_sigchld_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = parent_sigchld_signal_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int connect_parent_sigint_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = parent_termination_signal_handler;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int connect_parent_signal_handlers()
{
    if (connect_parent_start_signal_handler() == -1)
    {
        return -1;
    }

    if (connect_parent_termination_signal_handler() == -1)
    {
        return -1;
    }

    if (connect_parent_sigint_signal_handler() == -1)
    {
        return -1;
    }

    return 0;
}

int start_workers()
{
    living_children++;
    sender_worker_pid = fork();
    if (sender_worker_pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (sender_worker_pid == 0)
    {
        int result = sender_worker();
        exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        return -1;
    }

    living_children++;
    receiver_worker_pid = fork();
    if (receiver_worker_pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (receiver_worker_pid == 0)
    {
        int result = receiver_worker();
        exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        return -1;
    }

    if (connect_parent_signal_handlers() == -1)
    {
        return -1;
    }

    return 0;
}

int is_server_alive(int server_pid)
{
    char server_fifo_path[MAX_SERVER_FIFO_PATH_LENGTH];
    return does_fifo_exist(get_server_fifo_path(server_pid, server_fifo_path));
}

int send_connection_request(int server_pid, int nonblock)
{
    char server_fifo_path[MAX_SERVER_FIFO_PATH_LENGTH];
    char connection_request[MAX_CONNECTION_REQUEST_LENGTH];
    produce_connection_request(getpid(), connection_request, nonblock);

    char mutex_path[MAX_FILE_NAME_LENGTH];
    sem_t *sem = sem_open(get_server_fifo_mutex_path(server_pid, mutex_path), 0);
    if (sem == SEM_FAILED)
    {
        perror("sem_open");
        return -1;
    }
    sem_wait(sem);

    int server_fifo_fd = open(get_server_fifo_path(server_pid, server_fifo_path), O_WRONLY);
    if (server_fifo_fd == -1)
    {
        perror("open");
        sem_post(sem);
        sem_close(sem);
        return -1;
    }
    if (write(server_fifo_fd, connection_request, strlen(connection_request) + 1) == -1)
    {
        perror("write");
        sem_post(sem);
        sem_close(sem);
        return -1;
    }
    if (close(server_fifo_fd) == -1)
    {
        perror("close");
        sem_post(sem);
        sem_close(sem);
        return -1;
    }

    sem_post(sem);
    sem_close(sem);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <command> <server_pid>\n", argv[0]);
        printf("\t<command>\n");
        printf("\t\tconnect - send connection request and wait until there is available spot\n");
        printf("\t\ttryconnect - send connection request and exit immediately if there is no available spot\n");
    }

    char *command = argv[1];
    char *server_pid_string = argv[2];

    int server_pid = atoi(server_pid_string);

    if (server_pid <= 0)
    {
        printf("Invalid server PID\n");
        return 1;
    }

    if (!is_server_alive(server_pid))
    {
        printf("No open server with PID %d\n", server_pid);
        return 1;
    }

    if (strcmp(command, "connect") == 0)
    {
        printf("Client started\n");
        start_workers();
        send_connection_request(server_pid, 0);
    }
    else if (strcmp(command, "tryconnect") == 0)
    {
        printf("Client started\n");
        start_workers();
        send_connection_request(server_pid, 1);
    }
    else
    {
        printf("Invalid command\n");
        return 1;
    }

    printf("Sent connection request to server with PID %d\n", server_pid);
    printf("Waiting for response...\n");

    while (living_children)
    {
        sleep(PARENT_PROCESS_SLEEP_INTERVAL);
    }

    parent_cleanup();

    return 0;
}
