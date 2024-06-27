#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COMMAND_LENGTH 256
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 256

#define MAX_FILE_READ_LENGTH 256

#define SORT_STUDENT_NAME 0b01
#define SORT_GRADE 0b10
#define SORT_ASCENDING 0b01
#define SORT_DESCENDING 0b10

#define COMMAND_EXECUTED_SUCCESSFULLY 0
#define INVALID_COMMAND 1
#define ERROR_EXECUTING_COMMAND 2
#define TOO_MANY_ARGUMENTS 3
#define COMMAND_FAILED 4
#define EMPTY_COMMAND 5
#define LOG_FILE "StudentGradeManagementSystem.log"

int log_file_descriptor;
char parsed_args[MAX_ARGS][MAX_ARG_LENGTH];

/**
 * Returns the length of the given string.
 *
 * @param string The string to measure.
 * @return The length of the string.
 */
int string_length(char *string)
{
    int length = 0;
    while (string[length] != '\0')
    {
        length++;
    }
    return length;
}

/**
 * Parses the arguments from the given command string.
 *
 * @param command The command string to parse.
 * @return An integer representing the number of arguments parsed.
 */
int parse_args(char *command)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int in_quotes = 0;
    while (command[i] == ' ')
    {
        i++;
    }
    if (command[i] == '\0')
    {
        return 0;
    }
    while (command[i] != '\0')
    {
        if (command[i] == '"')
        {
            in_quotes = !in_quotes;
        }
        else if (!in_quotes && command[i] == ' ')
        {
            while (command[i + 1] == ' ')
            {
                i++;
            }
            if (command[i + 1] == '\0')
            {
                break;
            }
            parsed_args[j][k] = '\0';
            j++;
            k = 0;
        }
        else
        {
            parsed_args[j][k] = command[i];
            k++;
        }
        i++;
    }
    parsed_args[j][k] = '\0';
    return j + 1;
}

/**
 * Checks if the given strings are equal.
 *
 * @param string1 The first string to compare.
 * @param string2 The second string to compare.
 * @param length The length of the strings.
 * @return 1 if the strings are equal, 0 otherwise.
 */
int are_equal(char *string1, char *string2, int length)
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

/**
 * Finds the first occurrence of a substring within a given string.
 *
 * @param string The string to search within.
 * @param substring The substring to search for.
 * @param length The length of the string.
 * @return The index of the first occurrence of the substring within the string, or -1 if not found.
 */
int find_first(char *string, char *substring, int length)
{
    if (length <= 0 || string[0] == '\0')
    {
        return -1;
    }
    for (int i = 0; i < length; i++)
    {
        for (int j = 0; substring[j] != '\0'; j++)
        {
            if (string[i + j] != substring[j])
            {
                break;
            }
            if (substring[j + 1] == '\0')
            {
                return i;
            }
        }
    }
    return -1;
}

/**
 * Finds the last occurrence of a substring within a given string.
 *
 * @param string The string to search within.
 * @param substring The substring to search for.
 * @param length The length of the string.
 * @return The index of the last occurrence of the substring within the string, or -1 if not found.
 */
int find_last(char *string, char *substring, int length)
{
    if (length <= 0 || string[0] == '\0')
    {
        return -1;
    }
    for (int i = length - 1; i >= 0; i--)
    {
        for (int j = 0; substring[j] != '\0'; j++)
        {
            if (string[i + j] != substring[j])
            {
                break;
            }
            if (substring[j + 1] == '\0')
            {
                return i;
            }
        }
    }
    return -1;
}

/**
 * Frees the memory allocated for an array of strings.
 *
 * @param array The array of strings to be freed.
 * @param length The length of the array.
 */
void free_string_array(char **array, int length)
{
    for (int i = 0; i < length; i++)
    {
        free(array[i]);
    }
    free(array);
}

/*===========================================*/

/**
 * Logs a command message.
 *
 * This function logs the specified command message to a log file.
 *
 * @param message The command message to be logged.
 * @param length The length of the command message.
 * @return 0 if the command message was logged successfully, -1 otherwise.
 */
int log_command(char *message, int length)
{
    if (write(log_file_descriptor, message, length) == -1)
    {
        perror("Error writing to log file");
        return -1;
    }
    return 0;
}

/*===========================================*/

/**
 * Creates a grade file with the specified file name.
 *
 * @param file_name The name of the grade file to create.
 * @return 0 if the grade file is successfully created, -1 otherwise.
 */
int create_grade_file(char *file_name)
{
    int file_descriptor = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        return -1;
    };
    write(STDOUT_FILENO, "File created\n", 13);
    return 0;
}

/**
 * Searches for a student with the given name in the specified file.
 * If the student is found, the student's name and grade are printed to the standard output.
 *
 * @param file_name The name of the file to search in.
 * @param student_name The name of the student to search for.
 * @return 0 if student was found, -1 otherwise.
 */
int search_student(char *file_name, char *student_name)
{
    int file_descriptor = open(file_name, O_RDONLY);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    char buffer[MAX_FILE_READ_LENGTH];
    int read_bytes;
    int found = 0;
    int student_name_length = string_length(student_name);
    while ((read_bytes = read(file_descriptor, buffer, MAX_FILE_READ_LENGTH)) > 0)
    {
        buffer[read_bytes] = '\0';
        int index = find_first(buffer, student_name, read_bytes);

        while (
            index > 0 &&
            ((buffer[index - 1] != '\n' && buffer[index - 1] != ' ') ||
             buffer[index + student_name_length] != ' '))
        {
            index = find_first(buffer + index + 1, student_name, read_bytes - index - 1);
        }

        if (index == -1)
        {
            continue;
        }

        int linebreak_index = find_last(buffer, "\n", index);
        int next_linebreak_index = find_first(buffer + linebreak_index + 1, "\n", read_bytes - linebreak_index);
        write(STDOUT_FILENO, buffer + linebreak_index + 1, next_linebreak_index + 1);
        found = 1;
        break;
    }
    if (read_bytes == -1)
    {
        perror("Error reading file");
        return -1;
    }
    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        return -1;
    }
    if (!found)
    {
        write(STDOUT_FILENO, "No results found\n", 17);
    }
    return 0;
}

/**
 * Writes the student and their grade at the end of the file.
 *
 * @param filename The path to the file where the grades are stored.
 * @param student_name The name of the student.
 * @param grade The grade to be added for the student.
 * @return 0 if the grade was successfully added, -1 otherwise.
 */
int add_student_grade(char *filename, char *student_name, char *grade)
{
    int file_descriptor = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    if (write(file_descriptor, student_name, string_length(student_name)) == -1)
    {
        perror("Error writing to file");
        return -1;
    }
    if (write(file_descriptor, ", ", 2) == -1)
    {
        perror("Error writing to file");
        return -1;
    }
    if (write(file_descriptor, grade, string_length(grade)) == -1)
    {
        perror("Error writing to file");
        return -1;
    }
    if (write(file_descriptor, "\n", 1) == -1)
    {
        perror("Error writing to file");
        return -1;
    }
    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        return -1;
    }
    write(STDOUT_FILENO, "Grade added\n", 12);
    return 0;
}

/**
 * Displays all the contents of the specified file.
 *
 * @param filename The path of the file to be displayed.
 * @return 0 if file content is successfully printed, -1 otherwise.
 */
int show_all(char *filename)
{
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    char buffer[MAX_FILE_READ_LENGTH];
    int read_bytes;
    int is_empty = 1;
    while ((read_bytes = read(file_descriptor, buffer, MAX_FILE_READ_LENGTH)) > 0)
    {
        buffer[read_bytes] = '\0';
        write(STDOUT_FILENO, buffer, read_bytes);
        is_empty = 0;
    }
    if (read_bytes == -1)
    {
        perror("Error reading file");
        return -1;
    }
    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        return -1;
    }
    if (is_empty)
    {
        write(STDOUT_FILENO, "The file is empty\n", 18);
    }
    return 0;
}

/**
 * Displays a specific page of a file.
 *
 * @param filename The name of the file to display.
 * @param page_length The number of lines per page.
 * @param page_index The index of the page to display.
 * @return 0 if the page was displayed successfully, -1 otherwise.
 */
int show_page(char *filename, int page_length, int page_index)
{
    if (page_length <= 0 || page_index <= 0)
    {
        write(STDERR_FILENO, "Invalid page length or index\n", 29);
        if (log_command("Invalid page length or index\n", 29) == -1)
            return -1;
        return COMMAND_FAILED;
    }
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    char buffer[MAX_FILE_READ_LENGTH];
    int read_bytes;
    int lines = 0;
    while ((read_bytes = read(file_descriptor, buffer, MAX_FILE_READ_LENGTH)) > 0)
    {
        buffer[read_bytes] = '\0';
        int index = 0;
        int line_index = 0;
        while (index < read_bytes)
        {
            if (buffer[index] == '\n')
            {
                lines++;
                if (lines > page_length * (page_index - 1))
                {
                    write(STDOUT_FILENO, buffer + line_index, index - line_index + 1);
                }
                line_index = index + 1;
            }
            index++;
            if (lines == page_length * page_index)
            {
                break;
            }
        }
        if (lines == page_length * page_index)
        {
            break;
        }
        if (lines > page_length * (page_index - 1))
        {
            write(STDOUT_FILENO, buffer + line_index, index - line_index + 1);
        }
    }
    if (read_bytes == -1)
    {
        perror("Error reading file");
        return -1;
    }
    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        return -1;
    }
    if (lines <= page_length * (page_index - 1))
    {
        write(STDOUT_FILENO, "No results found\n", 17);
    }
    return 0;
}

/**
 * Displays the first five lines of the file specified by the given filename.
 *
 * @param filename The path to the file to be displayed.
 * @return 0 if the file is successfully displayed, -1 otherwise.
 */
int show_first_five(char *filename)
{
    return show_page(filename, 5, 1);
}

/**
 * Displays the contents of a file in sorted order based on specified criteria.
 *
 * @param filename The path of the file to be sorted.
 * @param sort_by The criteria to sort the file by.
 * @param sort_in The order in which to sort the file.
 * @return 0 if the sorting is successful, -1 otherwise.
 */
int sort_all(char *filename, int sort_by, int sort_in)
{
    if (sort_by != SORT_STUDENT_NAME && sort_by != SORT_GRADE)
    {
        write(STDERR_FILENO, "Invalid sort by\n", 16);
        if (log_command("Invalid sort by\n", 16) == -1)
            return -1;
        return COMMAND_FAILED;
    }
    if (sort_in != SORT_ASCENDING && sort_in != SORT_DESCENDING)
    {
        write(STDERR_FILENO, "Invalid sort in\n", 16);
        if (log_command("Invalid sort in\n", 16) == -1)
            return -1;
        return COMMAND_FAILED;
    }
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor == -1)
    {
        perror("Error opening file");
        return -1;
    }
    char buffer[MAX_FILE_READ_LENGTH];
    int read_bytes;
    int lines_length = 256;
    int lines_index = 0;
    char **lines = (char **)calloc(lines_length, sizeof(char *));
    while ((read_bytes = read(file_descriptor, buffer, MAX_FILE_READ_LENGTH)) > 0)
    {
        buffer[read_bytes] = '\0';
        int index = 0;
        int line_index = 0;
        while (index < read_bytes)
        {
            if (buffer[index] == '\n')
            {
                if (lines_index == lines_length)
                {
                    lines_length *= 2;
                    lines = (char **)realloc(lines, lines_length * sizeof(char *));
                }
                lines[lines_index] = (char *)calloc(index - line_index + 2, sizeof(char));
                for (int i = line_index; i <= index; i++)
                {
                    lines[lines_index][i - line_index] = buffer[i];
                }
                lines[lines_index][index - line_index + 1] = '\0';
                lines_index++;
                line_index = index + 1;
            }
            index++;
        }
        if (line_index < read_bytes)
        {
            if (lseek(file_descriptor, line_index - read_bytes, SEEK_CUR) == -1)
            {
                perror("Error seeking file");
                free_string_array(lines, lines_index);
                return -1;
            }
        }
    }

    if (read_bytes == -1)
    {
        perror("Error reading file");
        free_string_array(lines, lines_index);
        return -1;
    }

    if (close(file_descriptor) == -1)
    {
        perror("Error closing file");
        free_string_array(lines, lines_index);
        return -1;
    }

    if (sort_by == SORT_STUDENT_NAME)
    {
        for (int i = 0; i < lines_index; i++)
        {
            for (int j = i + 1; j < lines_index; j++)
            {
                if (sort_in == SORT_ASCENDING)
                {
                    if (strcmp(lines[i], lines[j]) > 0)
                    {
                        char *temp = lines[i];
                        lines[i] = lines[j];
                        lines[j] = temp;
                    }
                }
                else
                {
                    if (strcmp(lines[i], lines[j]) < 0)
                    {
                        char *temp = lines[i];
                        lines[i] = lines[j];
                        lines[j] = temp;
                    }
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < lines_index; i++)
        {
            for (int j = i + 1; j < lines_index; j++)
            {
                char *i_grade = lines[i] + find_first(lines[i], ", ", string_length(lines[i])) + 2;
                char *j_grade = lines[j] + find_first(lines[j], ", ", string_length(lines[j])) + 2;
                if (sort_in == SORT_ASCENDING)
                {
                    if (strcmp(i_grade, j_grade) < 0)
                    {
                        char *temp = lines[i];
                        lines[i] = lines[j];
                        lines[j] = temp;
                    }
                }
                else
                {
                    if (strcmp(i_grade, j_grade) > 0)
                    {
                        char *temp = lines[i];
                        lines[i] = lines[j];
                        lines[j] = temp;
                    }
                }
            }
        }
    }

    for (int i = 0; i < lines_index; i++)
    {
        write(STDOUT_FILENO, lines[i], string_length(lines[i]));
    }

    free_string_array(lines, lines_index);
    return 0;
}

/**
 * Retrieves the sort_by value.
 *
 * This function takes a pointer to a string `sort_by` and returns an integer value.
 * It retrieves the sort_by value and performs some operations on it.
 *
 * @param sort_by A pointer to a string representing the sort_by value.
 * @return An integer value representing the sort_by value.
 */
int get_sort_by(char *sort_by)
{
    if (are_equal(sort_by, "n\0", 2) || are_equal(sort_by, "name\0", 5))
    {
        return SORT_STUDENT_NAME;
    }
    else if (are_equal(sort_by, "g\0", 2) || are_equal(sort_by, "grade\0", 6))
    {
        return SORT_GRADE;
    }
    else
    {
        return -1;
    }
}

/**
 * Retrieves the sort_in value.
 *
 * This function takes a pointer to a character array `sort_in` and retrieves the value of `sort_in`.
 *
 * @param sort_in A pointer to a character array that stores the sort_in value.
 * @return An integer value representing the sort_in value.
 */
int get_sort_in(char *sort_in)
{
    if (are_equal(sort_in, "a\0", 2) || are_equal(sort_in, "ascending\0", 10))
    {
        return SORT_ASCENDING;
    }
    else if (are_equal(sort_in, "d\0", 2) || are_equal(sort_in, "descending\0", 11))
    {
        return SORT_DESCENDING;
    }
    else
    {
        return -1;
    }
}

/**
 * Displays the available commands.
 */
int show_commands()
{
    write(STDOUT_FILENO, "gtuStudentGrades\n", 17);
    write(STDOUT_FILENO, "\tShows available commands\n", 26);

    write(STDOUT_FILENO, "gtuStudentGrades <filename>\n", 28);
    write(STDOUT_FILENO, "\tCreates a new grade file\n", 26);

    write(STDOUT_FILENO, "searchStudent <filename> <studentName>\n", 39);
    write(STDOUT_FILENO, "\tSearches for a student in the grade file\n", 42);

    write(STDOUT_FILENO, "addStudentGrade <filename> <studentName> <grade>\n", 49);
    write(STDOUT_FILENO, "\tAdds a new student grade to the grade file\n", 44);

    write(STDOUT_FILENO, "showAll <filename>\n", 19);
    write(STDOUT_FILENO, "\tShows all students and their grades\n", 37);

    write(STDOUT_FILENO, "listGrades <filename>\n", 22);
    write(STDOUT_FILENO, "\tShows the first 5 students and their grades\n", 45);

    write(STDOUT_FILENO, "listSome <filename> <pageLength> <pageIndex>\n", 45);
    write(STDOUT_FILENO, "\tShows a page of students and their grades\n", 43);
    write(STDOUT_FILENO, "\tpageLength: An integer greater than 0\n", 39);
    write(STDOUT_FILENO, "\tpageIndex: An integer greater than 0\n", 38);

    write(STDOUT_FILENO, "sortAll <filename> <sortBy> <sortIn>\n", 37);
    write(STDOUT_FILENO, "\tShows all students and their grades in sorted order\n", 53);
    write(STDOUT_FILENO, "\tsortBy: n/name or g/grade\n", 27);
    write(STDOUT_FILENO, "\tsortIn: a/ascending or d/descending\n", 37);

    return 0;
}

/**
 * Executes a command.
 *
 * This function takes the number of command line arguments as input and performs the necessary actions to execute the command.
 *
 * @param argc The number of command line arguments.
 * @return The exit status of the command.
 */
int execute_command(int argc)
{
    int result = INVALID_COMMAND;
    if (argc == 0)
    {
        result = EMPTY_COMMAND;
    }
    else if (argc == 1)
    {
        if (are_equal(parsed_args[0], "gtuStudentGrades\0", 17))
        {
            result = show_commands();
        }
        else
        {
            write(STDERR_FILENO, "Invalid command\n", 16);
        }
    }
    else if (argc == 2)
    {
        if (are_equal(parsed_args[0], "gtuStudentGrades\0", 17))
        {
            result = create_grade_file(parsed_args[1]);
        }
        else if (are_equal(parsed_args[0], "showAll\0", 8))
        {
            result = show_all(parsed_args[1]);
        }
        else if (are_equal(parsed_args[0], "listGrades\0", 11))
        {
            result = show_first_five(parsed_args[1]);
        }
        else
        {
            write(STDERR_FILENO, "Invalid command\n", 16);
        }
    }
    else if (argc == 3)
    {
        if (are_equal(parsed_args[0], "searchStudent\0", 14))
        {
            result = search_student(parsed_args[1], parsed_args[2]);
        }
        else
        {
            write(STDERR_FILENO, "Invalid command\n", 16);
        }
    }
    else if (argc == 4)
    {
        if (are_equal(parsed_args[0], "addStudentGrade\0", 16))
        {
            result = add_student_grade(parsed_args[1], parsed_args[2], parsed_args[3]);
        }
        else if (are_equal(parsed_args[0], "listSome\0", 9))
        {
            result = show_page(parsed_args[1], atoi(parsed_args[2]), atoi(parsed_args[3]));
        }
        else if (are_equal(parsed_args[0], "sortAll\0", 8))
        {
            result = sort_all(parsed_args[1], get_sort_by(parsed_args[2]), get_sort_in(parsed_args[3]));
        }
        else
        {
            write(STDERR_FILENO, "Invalid command\n", 16);
        }
    }
    else
    {
        write(STDERR_FILENO, "Too many arguments\n", 20);
        result = TOO_MANY_ARGUMENTS;
    }

    if (result == COMMAND_FAILED)
    {
        return COMMAND_FAILED;
    }
    else if (result == -1)
    {
        return ERROR_EXECUTING_COMMAND;
    }
    else if (result == 0)
    {
        return COMMAND_EXECUTED_SUCCESSFULLY;
    }

    return result;
}

int main(int argc, char *argv[])
{
    log_file_descriptor = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (log_file_descriptor == -1)
    {
        perror("Error opening log file");
        return -1;
    }
    if (log_command(">>> ", 4) == -1)
    {
        return -1;
    }
    if (log_command(argv[0], string_length(argv[0])) == -1)
    {
        return -1;
    }
    if (log_command("\n", 1) == -1)
    {
        return -1;
    }
    int result = execute_command(parse_args(argv[0]));
    if (result == EMPTY_COMMAND)
    {
        if (log_command("Empty command\n", 14) == -1)
        {
            return -1;
        }
        if (close(log_file_descriptor) == -1)
        {
            perror("Error closing log file");
            return -1;
        }
        return 0;
    }
    else if (result == INVALID_COMMAND)
    {
        if (log_command("Invalid command\n", 16) == -1)
        {
            return -1;
        }
    }
    else if (result == ERROR_EXECUTING_COMMAND)
    {
        if (log_command("Error executing command\n", 24) == -1)
        {
            return -1;
        }
    }
    else if (result == COMMAND_EXECUTED_SUCCESSFULLY)
    {
        if (log_command("Command executed successfully\n", 30) == -1)
        {
            return -1;
        }
    }
    else if (result == TOO_MANY_ARGUMENTS)
    {
        if (log_command("Too many arguments\n", 19) == -1)
        {
            return -1;
        }
    }
    else if (result == COMMAND_FAILED)
    {
        if (log_command("Command failed\n", 15) == -1)
        {
            return -1;
        }
    }
    if (close(log_file_descriptor) == -1)
    {
        perror("Error closing log file");
        return -1;
    }
    return result;
}