#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> 

#define MAX_ARGUMENTS 7
#define MAX_BACKGROUND_PROCESSES 100
#define MAX_COMMAND_LENGTH 1024

int background_process_count = 0;
pid_t background_process_list[MAX_BACKGROUND_PROCESSES];
pid_t foreground_process_pid = -1;
struct rusage accumulated_resource_usage = {0}; 

void handle_sigint(int signal_number);
void handle_sigtstp(int signal_number);
void handle_sigchld(int signal_number);
int execute_builtin_command(char *arguments[]);
void execute_external_command(char *arguments[], int run_in_background);
int handle_redirection(char *arguments[], int *run_in_background);
int handle_piping(char *arguments[]);

/*
 * main function
 * handles main loop, input, and commands
 */
int main() {
    char user_input[MAX_COMMAND_LENGTH];
    char *command_arguments[MAX_ARGUMENTS];
    int run_in_background;

    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGCHLD, handle_sigchld);

    printf("Welcome to Dragon Shell!\n\n");

    while (1) {
        printf("dragonshell> ");
        fflush(stdout);

        if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
            break;
        }

        user_input[strlen(user_input) - 1] = '\0';
        int argument_index = 0;
        command_arguments[argument_index] = strtok(user_input, " ");
        while (command_arguments[argument_index] != NULL && argument_index < MAX_ARGUMENTS - 1) {
            command_arguments[++argument_index] = strtok(NULL, " ");
        }
        command_arguments[MAX_ARGUMENTS - 1] = NULL;

        if (command_arguments[0] == NULL) {
            continue;
        }

        if (execute_builtin_command(command_arguments)) {
            continue;
        }

        run_in_background = 0;

        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);

        handle_redirection(command_arguments, &run_in_background);

        if (handle_piping(command_arguments)) {
            dup2(saved_stdin, STDIN_FILENO);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdin);
            close(saved_stdout);
            continue;
        }

        execute_external_command(command_arguments, run_in_background);

        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdin);
        close(saved_stdout);
    }

    return 0;
}

/*
 * function to run cd, pwd, and exit
 * returns 1 if the command is built-in, 0 if not
 */
int execute_builtin_command(char *arguments[]) {
    if (strcmp(arguments[0], "cd") == 0) {
        if (arguments[1] == NULL) {
            printf("dragonshell: Expected argument to \"cd\"\n");
        } else if (chdir(arguments[1]) != 0) {
            perror("dragonshell");
        }
        return 1;
    } 
    else if (strcmp(arguments[0], "pwd") == 0) {
        char current_directory[MAX_COMMAND_LENGTH];
        if (getcwd(current_directory, sizeof(current_directory)) != NULL) {
            printf("%s\n", current_directory);
        } else {
            perror("dragonshell");
        }
        return 1;
    } 
    else if (strcmp(arguments[0], "exit") == 0) {
        for (int i = 0; i < background_process_count; i++) {
            if (kill(background_process_list[i], SIGTERM) == 0) {
                struct rusage process_usage;
                int process_status;
                wait4(background_process_list[i], &process_status, 0, &process_usage);
                accumulated_resource_usage.ru_utime.tv_sec += process_usage.ru_utime.tv_sec;
                accumulated_resource_usage.ru_stime.tv_sec += process_usage.ru_stime.tv_sec;
            }
        }
        printf("User time: %ld seconds\n", accumulated_resource_usage.ru_utime.tv_sec);
        printf("Sys time: %ld seconds\n", accumulated_resource_usage.ru_stime.tv_sec);
        exit(0);
    }
    return 0; 
}

/*
 * function to run external commands
 * forks child process and handles execve
 */
void execute_external_command(char *arguments[], int run_in_background) {
    pid_t child_pid;
    struct rusage child_usage;
    char *environment[] = {NULL};

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
    } else if (child_pid == 0) {
        if (execve(arguments[0], arguments, environment) == -1) {
            if (strchr(arguments[0], '/') == NULL) {
                char command_with_path[MAX_COMMAND_LENGTH];
                snprintf(command_with_path, sizeof(command_with_path), "./%s", arguments[0]);
                if (execve(command_with_path, arguments, environment) == -1) {
                    fprintf(stderr, "dragonshell: Command not found\n");
                }
            } else {
                fprintf(stderr, "dragonshell: Command not found\n");
            }
        }
        exit(EXIT_FAILURE);
    } else {
        if (!run_in_background) {
            foreground_process_pid = child_pid;
            int process_status;
            wait4(child_pid, &process_status, WUNTRACED, &child_usage);
            accumulated_resource_usage.ru_utime.tv_sec += child_usage.ru_utime.tv_sec;
            accumulated_resource_usage.ru_stime.tv_sec += child_usage.ru_stime.tv_sec;


            foreground_process_pid = -1;
        } else {
            background_process_list[background_process_count++] = child_pid;
            fprintf(stderr, "PID %d is sent to background\n", child_pid);
        }
    }
}

/*
 * function to run input and output redirection
 */
int handle_redirection(char *arguments[], int *run_in_background) {
    int input_file_descriptor = -1, output_file_descriptor = -1, i, j;

    for (i = 0; arguments[i] != NULL; i++) {
        if (strcmp(arguments[i], "<") == 0) {
            input_file_descriptor = open(arguments[i + 1], O_RDONLY);
            if (input_file_descriptor < 0) {
                perror("Error opening input file");
                return -1;
            }
            dup2(input_file_descriptor, STDIN_FILENO);
            close(input_file_descriptor);

            for (j = i; arguments[j + 2] != NULL; j++) {
                arguments[j] = arguments[j + 2];
            }
            arguments[j] = NULL;
            i--;
        } else if (strcmp(arguments[i], ">") == 0) {
            output_file_descriptor = open(arguments[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_file_descriptor < 0) {
                perror("Error opening output file");
                return -1;
            }
            dup2(output_file_descriptor, STDOUT_FILENO);
            close(output_file_descriptor);

            for (j = i; arguments[j + 2] != NULL; j++) {
                arguments[j] = arguments[j + 2];
            }
            arguments[j] = NULL;
            i--;
        } else if (strcmp(arguments[i], "&") == 0) {
            *run_in_background = 1;
            arguments[i] = NULL;
            break;
        }
    }
    return 0;
}

/*
 * function to run pipes
 * separates command and runs first part in child process with output to second part
 */
int handle_piping(char *arguments[]) {
    int pipe_file_descriptors[2], child_exit_status;
    pid_t first_child_pid, second_child_pid;
    struct rusage first_child_usage, second_child_usage;
    int pipe_position = -1;

    for (int i = 0; arguments[i] != NULL; i++) {
        if (strcmp(arguments[i], "|") == 0) {
            pipe_position = i;
            arguments[i] = NULL;
            break;
        }
    }

    if (pipe_position == -1) return 0;

    if (pipe(pipe_file_descriptors) == -1) {
        perror("pipe");
        return -1;
    }

    first_child_pid = fork();
    if (first_child_pid < 0) {
        perror("fork");
        return -1;
    } else if (first_child_pid == 0) {
        close(pipe_file_descriptors[0]);
        dup2(pipe_file_descriptors[1], STDOUT_FILENO);
        close(pipe_file_descriptors[1]);
        if (execve(arguments[0], arguments, NULL) == -1) {
            perror("dragonshell");
            exit(EXIT_FAILURE);
        }
    }

    second_child_pid = fork();
    if (second_child_pid < 0) {
        perror("fork");
        return -1;
    } else if (second_child_pid == 0) {
        close(pipe_file_descriptors[1]);
        dup2(pipe_file_descriptors[0], STDIN_FILENO);
        close(pipe_file_descriptors[0]);
        if (execve(arguments[pipe_position + 1], &arguments[pipe_position + 1], NULL) == -1) {
            perror("dragonshell");
            exit(EXIT_FAILURE);
        }
    }

    close(pipe_file_descriptors[0]);
    close(pipe_file_descriptors[1]);

    wait4(first_child_pid, &child_exit_status, 0, &first_child_usage);
    accumulated_resource_usage.ru_utime.tv_sec += first_child_usage.ru_utime.tv_sec;
    accumulated_resource_usage.ru_stime.tv_sec += first_child_usage.ru_stime.tv_sec;

    wait4(second_child_pid, &child_exit_status, 0, &second_child_usage);
    accumulated_resource_usage.ru_utime.tv_sec += second_child_usage.ru_utime.tv_sec;
    accumulated_resource_usage.ru_stime.tv_sec += second_child_usage.ru_stime.tv_sec;

    return 1;
}

/*
 * handle SIGSTP when pressing CONTROL+Z 
 * suspends foreground processes
 */
void handle_sigtstp(int signal_number) {
    if (foreground_process_pid > 0) {
        printf("\n");
        kill(foreground_process_pid, SIGTSTP);
    }
}

/* 
 * handle SIGINT when pressing CONTROL + C
 * terminates all processes
 */
void handle_sigint(int signal_number) {
    printf("\n");
}

/*
 * handle SIGCHLD
 * terminates background processes
 */
void handle_sigchld(int signal_number) {
    int child_exit_status;
    pid_t child_pid;
    struct rusage child_resource_usage;

    while ((child_pid = wait4(-1, &child_exit_status, WNOHANG, &child_resource_usage)) > 0) {
        accumulated_resource_usage.ru_utime.tv_sec += child_resource_usage.ru_utime.tv_sec;
        accumulated_resource_usage.ru_stime.tv_sec += child_resource_usage.ru_stime.tv_sec;

        for (int i = 0; i < background_process_count; i++) {
            if (background_process_list[i] == child_pid) {
                for (int j = i; j < background_process_count - 1; j++) {
                    background_process_list[j] = background_process_list[j + 1];
                }
                background_process_count--;
                break;
            }
        }
    }
}
