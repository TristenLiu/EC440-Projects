#include "threads.h"
#include <stdio.h>
#include <string.h>

#define NUM_THREADS 5

void* testfunc()
{
	printf("thread %d in test func\n", (int) pthread_self());

    //char* test0 = "hello";
    int *test1 = malloc(sizeof(int));
    *test1 = 5;

	return (void*) test1;
}

int main()
{
    pthread_t th;

    pthread_create(&th, NULL, &testfunc, NULL);

    int* retval;

    printf("returning\n");
    pthread_join(th, (void**) &retval);

    printf("retval returned: %d\n", *retval);

    free(retval);

    return 0;
}