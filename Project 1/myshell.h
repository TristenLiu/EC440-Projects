#ifndef MYSHELL_H
#define MYSHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

char** parse_input(char* line, int* numcommands, char* delim);
//parse through the input line separating by the delim parameter
//return char** pointing to each separated string

void killspaces(char* input);
//kill all trailing spaces by replacing them with '\0'

// void readfile(char*** args, char* file_name, int* num_args);
// //read the file and concatenate the file contents into args

#endif