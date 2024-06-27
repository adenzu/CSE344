#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "eclist.h"
#include "shared.h"

char *server_directory_path = NULL;
char server_fifo_path[MAX_SERVER_FIFO_PATH_LENGTH] = "\0";

int server_fifo_fd;

int *worker_pids = NULL;
int max_clients;

sem_t *connection_mutex = NULL;

struct double_linkedlist *client_queue = NULL;

int serve_next_client();

int free_worker(int worker_pid)
{
    for (int i = 0; i < max_clients; i++)
    {
        if (worker_pids[i] == worker_pid)
        {
            printf("Client%d disconnected\n", i);
            worker_pids[i] = 0;
            return 0;
        }
    }
    return -1;
}

void sigchld_handler(int signum)
{
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            // WARNING: Relatively costly operation in a signal handler.
            free_worker(pid);
            serve_next_client();
        }
    }
}

int connect_sigchld_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int get_number_of_clients()
{
    int number_of_clients = 0;
    for (int i = 0; i < max_clients; i++)
    {
        if (worker_pids[i] != 0)
        {
            number_of_clients++;
        }
    }
    return number_of_clients;
}

void reset_worker_pids()
{
    free(worker_pids);
    worker_pids = (int *)malloc(max_clients * sizeof(int));
    for (int i = 0; i < max_clients; i++)
    {
        worker_pids[i] = 0;
    }
}

int get_available_worker_index()
{
    for (int i = 0; i < max_clients; i++)
    {
        if (worker_pids[i] == 0)
        {
            return i;
        }
    }
    return -1;
}

int cleanup_dynamic_memory()
{
    free_double_linkedlist(client_queue);
    return 0;
}

int close_server_fifo()
{
    if (close(server_fifo_fd) == -1)
    {
        // WARNING: errno is a global variable that can be modified by
        // other functions in between the call to close() and checking
        // its value.
        if (errno == EBADF)
        {
            // When close_server_fifo() is called due to a SIGINT signal,
            // the process may be blocked in the open() function. Since
            // the file descriptor does not point to an open file description,
            // the close() function will return -1 with errno set to EBADF.
            // But this should not be considered an error.
            return 0;
        }
        perror("close");
        return -1;
    }
    return 0;
}

int cleanup_server_fifo()
{
    if (close_server_fifo() == -1)
    {
        return -1;
    }
    if (unlink(server_fifo_path) == -1)
    {
        perror("unlink");
        return -1;
    }
    return 0;
}

int is_client_queue_empty()
{
    return client_queue->size == 0;
}

int create_decliner(int client_pid, int decline_reason)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (pid == 0)
    {
        char client_pid_string[PID_STRING_LENGTH];
        char decline_reason_string[MAX_DECLINE_REASON_NUMBER_DIGITS];
        sprintf(client_pid_string, "%d", client_pid);
        sprintf(decline_reason_string, "%d", decline_reason);
        char *argv[] = {"./decliner", client_pid_string, decline_reason_string, NULL};
        char *envp[] = {NULL};
        execve("./decliner", argv, envp);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    return pid;
}

int decline_client(int client_pid, int decline_reason)
{
    int result = create_decliner(client_pid, decline_reason);
    if (result == -1)
    {
        return result;
    }
    return 0;
}

int decline_clients_in_queue(int decline_reason)
{
    while (!is_client_queue_empty())
    {
        int client_pid = pop_head(client_queue);
        decline_client(client_pid, decline_reason);
    }
    return 0;
}

void cleanup()
{
    decline_clients_in_queue(DECLINE_REASON_SERVER_TERMINATED);
    for (int i = 0; i < max_clients; i++)
    {
        if (worker_pids[i] != 0)
        {
            kill(worker_pids[i], SIGINT);
            waitpid(worker_pids[i], NULL, 0);
        }
    }
    cleanup_server_fifo();
    cleanup_dynamic_memory();
    sem_close(connection_mutex);
    char mutex_path[MAX_FILE_NAME_LENGTH];
    sem_unlink(get_server_fifo_mutex_path(getpid(), mutex_path));
}

void sigint_handler(int signum)
{
    cleanup();
    printf("Server exiting...\n");
    exit(EXIT_SUCCESS);
}

char *get_this_server_fifo_path()
{
    if (server_fifo_path[0] == '\0')
    {
        return get_server_fifo_path(getpid(), server_fifo_path);
    }
    return server_fifo_path;
}

int prepare_server_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        if (errno == ENOENT)
        {
            if (mkdir(path, 0777) == -1)
            {
                perror("mkdir");
                return -1;
            }
        }
        else
        {
            perror("stat");
            return -1;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        printf("Path %s is not a directory\n", path);
        return -1;
    }
    return 0;
}

int create_server_fifo()
{
    if (mkfifo(get_this_server_fifo_path(), 0666) == -1)
    {
        perror("mkfifo");
        return -1;
    }
    return 0;
}

int open_server_fifo_for_reading()
{
    server_fifo_fd = open(get_this_server_fifo_path(), O_RDONLY);
    if (server_fifo_fd == -1)
    {
        perror("open");
        return -1;
    }
    return 0;
}

int restart_server_fifo()
{
    if (close_server_fifo() == -1)
    {
        return -1;
    }
    if (open_server_fifo_for_reading() == -1)
    {
        return -1;
    }
    return 0;
}

int create_worker(int client_pid)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (pid == 0)
    {
        char client_pid_string[PID_STRING_LENGTH];
        sprintf(client_pid_string, "%d", client_pid);
        char *argv[] = {"./worker", client_pid_string, server_directory_path, NULL};
        char *envp[] = {NULL};
        execve("./worker", argv, envp);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    return pid;
}

int serve_client(int client_pid)
{
    int worker_index = get_available_worker_index();
    if (worker_index == -1)
    {
        return -1;
    }
    printf("Client PID %d connected as 'Client%d'\n", client_pid, worker_index);
    worker_pids[worker_index] = create_worker(client_pid);
    return 0;
}

int serve_next_client()
{
    if (is_client_queue_empty())
    {
        return 0;
    }
    int client_pid = pop_head(client_queue);
    if (serve_client(client_pid) == -1)
    {
        // WARNING: Should the client be reinserted into the queue?
        insert_head(client_queue, client_pid);
        return -1;
    }
    return 0;
}

void queue_up_client(int client_pid)
{
    insert_tail(client_queue, client_pid);
}

int dispatcher()
{
    if (create_server_fifo() == -1)
    {
        return -1;
    }

    printf("Waiting for clients...\n");

    if (open_server_fifo_for_reading() == -1)
    {
        cleanup_server_fifo();
        return -1;
    }

    int read_bytes;
    char buffer[SERVER_FIFO_BUFFER_SIZE];
    while (1)
    {
        while (!is_client_queue_empty() && serve_next_client() != -1)
            ;

        // WARNING: Race condition when more than one client
        // sends connection request at the same time.
        while ((read_bytes = read(server_fifo_fd, buffer, sizeof(buffer))) == -1 && errno == EINTR)
            ;
        if (read_bytes == -1)
        {
            perror("read");
            return -1;
        }

        if (read_bytes == 0)
        {
            if (restart_server_fifo() == -1)
            {
                return -1;
            }
            continue;
        }

        if (!is_connection_request(buffer))
        {
            printf("Received message: %s\n", buffer);
            continue;
        }

        int client_pid = get_connection_request_client_pid(buffer);
        printf("Received connection request from PID %d\n", client_pid);

        while (!is_client_queue_empty() && serve_next_client() != -1)
            ;
        if (serve_client(client_pid) == -1)
        {
            // WARNING: Not really checking if the failure of serving the client
            // was due to the capacity. It could be due to other reasons.
            printf("The server is at full capacity\n");
            if (is_connection_request_blocking(buffer))
            {
                printf("Adding the PID %d to the client queue list\n", client_pid);
                queue_up_client(client_pid);
            }
            else if (is_connection_request_nonblocking(buffer))
            {
                printf("Declining connection request of PID %d\n", client_pid);
                decline_client(client_pid, DECLINE_REASON_CAPACITY);
            }
        }
    }

    return 0;
}

int connect_sigint_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

void reset_client_queue()
{
    free_double_linkedlist(client_queue);
    client_queue = create_double_linkedlist();
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <directory_path> <maximum_number_of_clients>\n", argv[0]);
        return 1;
    }

    char *directory_path = argv[1];
    char *maximum_number_of_clients_string = argv[2];

    max_clients = atoi(maximum_number_of_clients_string);
    if (max_clients <= 0)
    {
        printf("Invalid maximum number of clients\n");
        return 1;
    }

    if (prepare_server_directory(directory_path) == -1)
    {
        return 1;
    }

    server_directory_path = directory_path;

    char mutex_path[MAX_FILE_NAME_LENGTH];
    connection_mutex = sem_open(get_server_fifo_mutex_path(getpid(), mutex_path), O_CREAT | O_EXCL, 0666, 1);
    if (connection_mutex == SEM_FAILED)
    {
        perror("sem_open");
        return 1;
    }

    if (connect_sigint_signal_handler() == -1)
    {
        return 1;
    }

    if (connect_sigchld_signal_handler() == -1)
    {
        return 1;
    }

    reset_worker_pids();
    reset_client_queue();

    printf("Server started at PID %d\n", getpid());

    if (dispatcher() == -1)
    {
        return 1;
    }

    cleanup();

    return 0;
}
