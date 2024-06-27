#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define COMMAND_LENGTH 256

/**
 * Reads a command from the user and stores it in the provided buffer.
 *
 * @param command The buffer to store the command in.
 * @return The number of characters read, or -1 if an error occurred.
 */
int read_command(char *command)
{
    int read_bytes = read(STDIN_FILENO, command, COMMAND_LENGTH);
    if (read_bytes == -1)
    {
        return -1;
    }
    command[read_bytes - 1] = '\0';
    return read_bytes;
}

/**
 * Checks if the given command is a quit command.
 *
 * @param command The command to check.
 * @return 1 if the command is a quit command, 0 otherwise.
 */
int is_quit_command(char *command)
{
    return command[0] == 'q' &&
           command[1] == 'u' &&
           command[2] == 'i' &&
           command[3] == 't' &&
           (command[4] == '\0' || command[4] == '\n');
}

int main(int argc, char *argv[])
{
    char command[COMMAND_LENGTH];
    int read_bytes;
    int child_exit_status;
    int show_child_exit_status = 0;

    if (argc == 2)
    {
        if (
            argv[1][0] == '-' &&
            argv[1][1] == '-' &&
            argv[1][2] == 'd' &&
            argv[1][3] == 'e' &&
            argv[1][4] == 'b' &&
            argv[1][5] == 'u' &&
            argv[1][6] == 'g' &&
            argv[1][7] == '\0')
        {
            show_child_exit_status = 1;
        }
    }

    while (1)
    {
        write(STDOUT_FILENO, ">>> ", 4);

        read_bytes = read_command(command);
        if (read_bytes == -1)
        {
            perror("Error reading command");
            return -1;
        }

        if (is_quit_command(command))
        {
            break;
        }

        switch (fork())
        {
        case -1:
            perror("Error forking");
            return -1;
        case 0:
            if (execve("./executor.out", (char *[]){command, NULL}, NULL) == -1)
            {
                perror("Error initiating command executor");
                return -1;
            }
            break;
        default:
            if (wait(&child_exit_status) == -1)
            {
                perror("Error waiting for child process");
                return -1;
            }
            if (show_child_exit_status)
            {
                if (WIFEXITED(child_exit_status))
                {
                    printf("Child process exited with status %d\n", WEXITSTATUS(child_exit_status));
                }
                else if (WIFSIGNALED(child_exit_status))
                {
                    printf("Child process was terminated by signal %d\n", WTERMSIG(child_exit_status));
                }
                else if (WIFSTOPPED(child_exit_status))
                {
                    printf("Child process was stopped by signal %d\n", WSTOPSIG(child_exit_status));
                }
                else
                {
                    printf("Child process did not exit normally\n");
                }
            }
            break;
        }
    }

    return 0;
}