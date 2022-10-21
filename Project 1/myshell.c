#include "myshell.h"

char** parse_input(char* line, int* numfound, char* delim)
{
    // dynamically allocate an array to fit up to 512 seperated letters
    char** separated = malloc(512*sizeof(char*)); 
    int i = 1;
    char* old_lim = NULL;
    // pointer to first occurence of our delimiter
    char* lim = strpbrk(line, delim);

    // if the first char in line is a space, set to after the first delimeter
    const char first = line[0];
    if (first == ' ')
    {
        *separated = (line+1);
        if ((*delim == ' '))
        {
            //if the delimiter is already searching for ' ', then step the lim to the next delim because we skip one space
            lim = strpbrk(lim+1, delim);
        }
    }
    else *separated = line;

    while (lim != NULL && (lim+1 != '\0'))                         
    {
        // while there are still words to seperate
        // if the new lim is not a newline char or an empty space, then add to separated
        if (strcmp(lim+1,"\n") && strcmp(lim+1," ") && strlen(lim+1) != 0)
        {
        *(separated+i) = lim+1;
        i++;
        }
        // hold onto the previous delimiter
        old_lim = lim;                          
        // advance through the input after each delimiter found
        lim = strpbrk(lim+1, delim);           
        // indicate the end of each word with a null space
        *old_lim = '\0';                        
        //printf("      %d: %s\n",i, *(separated+i));
    }

    //replace the \n at the end of the last string with a null char
    if (strchr(*(separated+i-1),'\n') != NULL)
    {
        char* temp = strchr(*(separated+i-1),'\n');
        *temp = '\0';
    }

    *(separated+i) = NULL;
    *numfound = i;

    return separated;
}

void killspaces(char* input)
{
    int i = 1;
    while (*(input + strlen(input)-i) == ' ')
    {
        *(input + strlen(input)-i) = '\0';
        i++;
    }
}

// void readfile(char*** args, char* file_name, int* num_args)
// {
//     FILE* fp = fopen(file_name, "r");
//     char* input = malloc(512*sizeof(char));
//     char* readf = malloc(512*sizeof(char));

//     while (!feof(fp))
//     {
//         fgets(readf, 512, (FILE*)fp);
//         strcat(input, readf);
//     }
//     strcat(input, "\0");
//     //printf("%s\n", input);

//     *(*args + *num_args - 1) = input;

//     free(input);
//     free(readf);
// }