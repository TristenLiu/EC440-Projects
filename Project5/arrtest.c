#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main()
{
    int arr[10];
    char* buffer = malloc(sizeof(int)*10);

    int i;
    for (i = 0; i < 10; i++)
    {
        memcpy((void*) buffer + i * sizeof(int), (void*) &i, sizeof(int));
    }
    memcpy((void*) arr, (void*) buffer, sizeof(arr));

    printf("ARR length: %d\nContents: ", sizeof(arr));
    for (i = 0; i < 10; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free(buffer);

    return 0;
}