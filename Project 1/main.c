#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include "myshell.h"

void handle_sigchld(int sig)
{
    //Catch SIGCHLD so the child does not become a zombie
}

void printCommands(char **input)
{
    // print the comand line separating by pipe
    int it = -1;
    char **iterator = input;
    while (*(iterator + it) != NULL)
    {
        it += 1;
        while ((*(iterator + it + 1) != NULL) && (strcmp(*(iterator + it + 1), "|") != 0))
        {
            // stop printing right before the new line so there are no trailing spaces
            printf("%s-", *(iterator + it));
            it += 1;
        }
        // print the final input before the pipe, with no space afterwards
        printf("%s", *(iterator + it));
        it++;
        if (*(iterator + it) != NULL)
        {
            printf("\n");
        }
    }
    // if the previous print did not send a new line, print a new line
    if (strchr(*(iterator + it - 1), '\n') == NULL)
    {
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);

    int cmd_prompt = 1;

    if ((argc > 1) && (strcmp(argv[1], "-n\n") != 0))
    {
        cmd_prompt = 0;
    }
    char input[512];
    char *line;
    int status;

    // dedicated iterator var
    int i;

    // run the shell at least once and until the user presses Ctrl+D
    do
    {
        fflush(stdin);
        memset(input, 0, 256); // refresh input
        int bkgd = 0;
        if (cmd_prompt)
        {
            printf("my_shell$ ");
        }
        line = fgets(input, 512, stdin); // pull 512 characters of input from stdin
        
        // test if input is empty
        i = 0;
        int space = 1;
        while ((line[i]) != '\n')
        {
            char c = line[i];
            // check if the input line is more than just an empty line
            if (isspace(c) == 0)
            {
                space = 0;
                break;
            }
            i++;
        }
        // if the full line is a 'space' char, don't parse
        if (space)
            continue;

        if (strchr(line, '&') != NULL)
        {
            // replace  & with nullchar, since it should be at the end of the line and has no more value
            bkgd = 1;
            char *temp = strchr(line, '&');
            *temp = '\0';
        }

        if (bkgd)
        {
            pid_t pid_bkgd = fork();
            if (pid_bkgd > 0)
            {
                // background call, let the parent function skip everything and start next loop
                fflush(stdout);
                continue;
            }
            else if (pid_bkgd < 0)
            {
                printf("ERROR: Background fork failed\n");
            }
            // the child will execute all commands
        }

        // call function to parse through input line, seperating by "|"
        int num_cmd = 0;
        char **pipe_section = parse_input(line, &num_cmd, "|"); 

        if (num_cmd == 1)
        {
            // if there are no pipes in the input line, no pipes should be created
            // parse and run the line normally
            char **args = malloc(512 * sizeof(char *));
            char *file_in;
            char *file_out;
            int in_redir = 0;
            int out_redir = 0;
            int in_holder, out_holder, num_args;

            char **out_redir_check = parse_input(*(pipe_section), &out_holder, ">");
            char **in_redir_check = parse_input(*(pipe_section), &in_holder, "<");

            if (in_holder == 1)
            {
                // no input redirection found
                args = parse_input(*(pipe_section), &num_args, " ");
                file_in = NULL;
            }
            else if (in_holder == 2)
            {
                // input redirection found
                in_redir = 1;
                args = parse_input(*(in_redir_check), &num_args, " ");
                file_in = in_redir_check[1];

                while (*file_in == ' ')
                { // get rid of all leading spaces
                    file_in = file_in + 1;
                }
                killspaces(file_in);
            }
            else if (in_holder > 2)
            {
                // break loop if too many redirections
                printf("ERROR: Too many input redirections\n");
                free(args);
                free(in_redir_check);
                free(out_redir_check);
                continue;
            }

            if (out_holder == 1)
            {
                file_out = NULL;
            }
            else if (out_holder == 2)
            {
                // output redirection found
                out_redir = 1;
                file_out = out_redir_check[1];

                // if the filename is at the end of the input line, remove '\n'
                if (strchr(file_out, '\n') != NULL)
                {
                    char *temp = strchr(file_out, '\n');
                    *temp = '\0';
                }

                // get rid of all leading spaces
                while (*file_out == ' ')
                { 
                    file_out = file_out + 1;
                }

                killspaces(file_out);
            }
            else if (out_holder > 2)
            {
                printf("ERROR: Too many out redirections\n");
                free(args);
                free(in_redir_check);
                free(out_redir_check);
                continue;
            }

            fflush(stdout);
            // fork the process
            pid_t pid = fork();
            if ((pid == 0))
            {
                // In the child process
                if (in_redir)
                {
                    int input = open(file_in, O_RDONLY);
                    if (input == -1)
                        printf("ERROR: Input file failed to open\n");
                    if (dup2(input, STDIN_FILENO) == -1)
                        printf("ERROR: stdin dup failed\n");
                    if (close(input) == -1)
                        printf("ERROR: failed to close input\n");
                }
                if (out_redir)
                {
                    int output = open(file_out, O_WRONLY | O_CREAT, 0777);
                    if (output == -1)
                        printf("ERROR: Output file failed to open\n");
                    if (dup2(output, STDOUT_FILENO) == -1)
                        printf("ERROR: stdout dup failed\n");
                    if (close(output) == -1)
                        printf("ERROR: failed to close output\n");
                }

                if (execvp(args[0], args) == -1)
                {
                    perror("ERROR");
                }
                exit(11);
            }
            else if ((pid < 0))
            {
                perror("ERROR: FORK FAILED\n");
            }

            //Child process execs, so only the parent process reaches this point
            waitpid(pid, &status, 0);
            if (WEXITSTATUS(status) == 11)
            {
                printf("The child has failed to execute\n");
            }

            free(in_redir_check);
            free(out_redir_check);
            free(args);
        }

        // if there is a pipe, then num_cmd > 1
        else if (num_cmd > 1)
        {
            i = 0;

            //initialize all pipe file descriptors in a single array
            int pipefds[2 * (num_cmd-1)];
            while (i < 2 * (num_cmd-1))
            {
                // open all the pipes
                if (pipe(pipefds + i * 2) < 0)
                {
                    printf("ERROR: failed to open pipes");
                    continue;
                }
                i++;
            }

            i = 0;
            // run for each of the commands
            while (i < num_cmd) 
            {
                int num = 0;
                char **args = malloc(512 * sizeof(char *));

                pid_t pid1 = fork();
                if (pid1 == 0)
                {
                    if (i == 0)
                    {
                        // on pipe 1: check for '<', redirect STDOUT
                        int in_redir = 0;
                        char *file_in = malloc(256 * sizeof(char));
                        char **in_redir_check = parse_input(*(pipe_section+i), &num, "<");
                        if (num == 1)
                        { 
                            // no redirection found
                            args = parse_input(*(pipe_section+i), &num, " ");
                        }
                        else if (num == 2)
                        { 
                            // one redirection found
                            in_redir = 1;
                            args = parse_input(*(in_redir_check), &num, " ");
                            strcpy(file_in, in_redir_check[1]);
                            while (*file_in == ' ')
                            { 
                                // get rid of all leading spaces
                                file_in = file_in + 1;
                            }
                            killspaces(file_in);
                        }
                        else if (num > 2)
                        {
                            printf("ERROR: Too many input redirections\n");
                            free(args);
                            free(in_redir_check);
                            continue;
                        }
                        fflush(stdout);

                        // Check if we need to do input redirection
                        if (in_redir)
                        {
                            int input = open(file_in, O_RDONLY);
                            if (input == -1)
                                printf("ERROR: Input file failed to open\n");
                            if (dup2(input, STDIN_FILENO) == -1)
                                printf("ERROR: stdin dup failed\n");
                            if (close(input) == -1)
                                printf("ERROR: failed to close input\n");
                        }

                        if (close(pipefds[0]) == -1) printf("ERROR: failed to close STDin\n");
                        if (dup2(pipefds[i*2+1], STDOUT_FILENO) == -1) printf("ERROR: failed to redirect STDOUT in pipe0\n");

                        free(in_redir_check);
                        free(file_in);

                    }
                    else if (i > 0 && i < num_cmd - 1)
                    { // between pipes 2 to (n-1), redirect both STDIN and STDOUT
                        
                        args = parse_input(*(pipe_section + i), &num, " ");

                        if (dup2(pipefds[(i-1)*2], STDIN_FILENO) == -1) printf("ERROR: failed to redirect STDIN in pipe%d\n", i);
                        if (dup2(pipefds[(i*2)+1], STDOUT_FILENO) == -1) printf("ERROR: failed to redirect STDOUT from pipe%d to the next pipe\n", i);
                    }
                    else if (i == num_cmd - 1)
                    {
                        // on the last pipe, check for '>', redirect STDIN and STDOUT(if there is an output redirection)
                        int out_redir = 0;
                        char* file_out = malloc(256 * sizeof(char));

                        char **out_redir_check = parse_input(*(pipe_section + i), &num, ">");

                        if (num == 1)
                        { 
                            // no redirection found
                            args = parse_input(*(pipe_section + i), &num, " ");
                        } else if (num == 2)
                        { 
                            // one redirection found
                            out_redir = 1;
                            args = parse_input(*(out_redir_check), &num, " ");
                            strcpy(file_out, out_redir_check[1]);

                            //if theres a new line at the end of the filename, get rid of it
                            if (strchr(file_out, '\n') != NULL)
                            {
                                char *temp = strchr(file_out, '\n');
                                *temp = '\0';

                            }
                            
                            // get rid of all leading spaces
                            while (*file_out == ' ')
                            { 
                                file_out = file_out + 1;
                            }

                            killspaces(file_out);
                        } else if (num > 2)
                        {
                            printf("ERROR: Too many output redirections\n");
                            free(args);
                            free(out_redir_check);
                            continue;
                        }

                        if (out_redir)
                        {
                            int output = open(file_out, O_WRONLY | O_CREAT, 0777);
                            if (output == -1)
                                printf("Failed to open the output file\n");
                            dup2(output, STDOUT_FILENO);
                            close(output);
                        }

                        if (dup2(pipefds[(i-1)*2], STDIN_FILENO) == -1) printf("ERROR: failed to redirect STDIN in pipe%d\n", i);
                        if (close(pipefds[(2*(num_cmd-1)-1)]) == -1) printf("ERROR: Failed to close STDOUT\n");

                        free(out_redir_check);
                        free(file_out);
                    }
            
                    //close all the file descriptors at the end of the child function
                    int j = 0;
                    while (j < 2 * (num_cmd-1))
                    {
                        close(pipefds[j]);
                        j++;
                    }

                    //execute the parsed command
                    if (execvp(args[0], args) == -1)
                    {
                        printf("ERROR: execvp #%d failed \n", i);
                        exit(11);
                    }

                } else if (pid1 < 0) 
                {
                    printf("ERROR: Fork Failed");
                }
                free(args);
                i++;
            }

            // In the parent, close all of the file descriptors
            int j = 0;
            while (j < 2 * (num_cmd-1))
            {
                close(pipefds[j]);
                j++;
            }

            // wait for #num_cmd children, so they do not become zombies
            j = 0;
            while (j < num_cmd)
            {
                wait(&status);
                j++;
            }

        }
        free(pipe_section);
    } while(1);
}
