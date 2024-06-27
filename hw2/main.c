#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#define FIFO1 "sum_fifo"
#define FIFO2 "mul_fifo"

#define PARENT_SLEEP_TIME 2
#define CHILD_SLEEP_TIME 10

#define MULTIPLICATION_COMMAND "multiply"
#define MULTIPLICATION_COMMAND_LENGTH 9

#define MAX_COMMAND_LENGTH 64

int child_counter = 0;

void sigchld_handler(int signum)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("Process %d has finished with status %d\n", pid, WEXITSTATUS(status));
            child_counter++;
        }
    }
}

void clean_up()
{
    unlink(FIFO1);
    unlink(FIFO2);
}

void sigint_handler(int signum)
{
    printf("Ctrl+C detected. Cleaning up...\n");
    clean_up();
    exit(EXIT_FAILURE);
}

int send_array(char *fifo, int array_size)
{
    int fifo_fd = open(fifo, O_WRONLY);
    if (fifo_fd == -1)
    {
        perror("open");
        close(fifo_fd);
        return 1;
    }

    if (write(fifo_fd, &array_size, sizeof(int)) == -1)
    {
        perror("write");
        close(fifo_fd);
        return 1;
    }

    printf("Sent array:");
    for (int i = 1; i <= array_size; i++)
    {
        if (write(fifo_fd, &i, sizeof(int)) == -1)
        {
            perror("write");
            close(fifo_fd);
            return 1;
        }
        printf(" %d", i);
    }
    printf("\n");

    close(fifo_fd);
    return 0;
}

int send_command(char *fifo, char *command, int command_length)
{
    int fifo_fd = open(fifo, O_WRONLY);
    if (fifo_fd == -1)
    {
        perror("open");
        close(fifo_fd);
        return 1;
    }

    if (write(fifo_fd, command, command_length * sizeof(char)) == -1)
    {
        perror("write");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);
    return 0;
}

int check_string(char *string1, char *string2, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (string1[i] != string2[i])
        {
            return 0;
        }
    }
    return 1;
}

int is_multiplication(char *command, int length)
{
    if (length < MULTIPLICATION_COMMAND_LENGTH)
    {
        return 0;
    }
    return check_string(command, MULTIPLICATION_COMMAND, MULTIPLICATION_COMMAND_LENGTH);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <positive_integer>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0)
    {
        fprintf(stderr, "Argument must be a positive integer\n");
        return 1;
    }

    if (access(FIFO1, F_OK) != -1)
    {
        unlink(FIFO1);
    }
    if (access(FIFO2, F_OK) != -1)
    {
        unlink(FIFO2);
    }
    if (mkfifo(FIFO1, 0666) == -1)
    {
        perror("mkfifo");
        return 1;
    }
    if (mkfifo(FIFO2, 0666) == -1)
    {
        perror("mkfifo");
        return 1;
    }

    struct sigaction sa_chld, sa_int;

    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    pid_t pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork");
        return 1;
    }
    else if (pid1 == 0)
    { // First child process
        int fifo1_fd = open(FIFO1, O_RDONLY);
        if (fifo1_fd == -1)
        {
            perror("open");
            return 1;
        }

        sleep(CHILD_SLEEP_TIME);

        int array_size = 0;
        if (read(fifo1_fd, &array_size, sizeof(int)) == -1)
        {
            perror("read");
            return 1;
        }

        int negative_sum = 0;
        int *buffer = (int *)calloc(array_size, sizeof(int));
        if (read(fifo1_fd, buffer, array_size * sizeof(int)) == -1)
        {
            perror("read");
            return 1;
        }

        for (int i = 0; i < array_size; i++)
        {
            negative_sum -= buffer[i];
        }

        close(fifo1_fd);

        int fifo2_fd = open(FIFO2, O_WRONLY);
        if (fifo2_fd == -1)
        {
            perror("open");
            return 1;
        }

        if (write(fifo2_fd, &negative_sum, sizeof(int)) == -1)
        {
            perror("write");
            return 1;
        }

        close(fifo2_fd);
        exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("fork");
        return 1;
    }
    else if (pid2 == 0)
    { // Second child process
        int fifo2_fd = open(FIFO2, O_RDONLY);
        if (fifo2_fd == -1)
        {
            perror("open");
            return 1;
        }

        sleep(CHILD_SLEEP_TIME);

        int array_size = 0;
        if (read(fifo2_fd, &array_size, sizeof(int)) == -1)
        {
            perror("read");
            return 1;
        }

        int product = 1;
        int *buffer = (int *)calloc(array_size, sizeof(int));
        for (int i = 0; i < array_size; i++)
        {
            if (read(fifo2_fd, buffer + i, sizeof(int)) == -1)
            {
                perror("read");
                return 1;
            }
        }

        char *command = (char *)calloc(MAX_COMMAND_LENGTH, sizeof(char));
        int command_length;
        for (int i = 0; i < MAX_COMMAND_LENGTH; i++)
        {
            if (read(fifo2_fd, command + i, sizeof(char)) == -1)
            {
                perror("read");
                return 1;
            }
            if (command[i] == '\0')
            {
                command_length = i + 1;
                break;
            }
        }

        if (is_multiplication(command, command_length) == 0)
        {
            fprintf(stderr, "Invalid command\n");
            return 1;
        }

        for (int i = 0; i < array_size; i++)
        {
            product *= buffer[i];
        }

        int negative_sum = 0;
        while (negative_sum >= 0)
        {
            if (read(fifo2_fd, &negative_sum, sizeof(int)) == -1)
            {
                perror("read");
                return 1;
            }
        }

        int final_result = product - negative_sum;
        printf("Result: %d\n", final_result);

        close(fifo2_fd);
        exit(EXIT_SUCCESS);
    }

    // Parent process
    int result;
    result = send_array(FIFO1, n);
    if (result)
    {
        return result;
    }

    result = send_array(FIFO2, n);
    if (result)
    {
        return result;
    }

    result = send_command(FIFO2, MULTIPLICATION_COMMAND, MULTIPLICATION_COMMAND_LENGTH);
    if (result)
    {
        return result;
    }

    while (child_counter < 2)
    {
        printf("Proceeding\n");
        sleep(PARENT_SLEEP_TIME);
    }

    clean_up();

    return 0;
}
