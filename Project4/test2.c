#include "tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

void* tls_tester0(void* arg)
{
    printf("creating thread 1\n");
    char buffer[1000];
    int i;

    printf("1: reading from non-existent TLS\n");
    if (tls_read(0, 100, buffer) == -1) printf("returned -1, read failed\n");
    else  printf("error: read should not have succeeded\n");

    printf("1: creating a TLS of size 6000\n");
    if (tls_create(6000) == 0) printf("tls_create succeeded\n");
    else printf("tls_create failed\n");

    printf("1: attempting to write to end of TLS\n");
    if (tls_write(4095,3, "abc") == 0) printf("write succeeded\n");
    else printf("write failed\n");

    printf("1: reading from previous write\n");
    if (tls_read(4095, 3, buffer) == 0) printf("read succeeded: %s\n", buffer);
    else printf("read failed\n");

    for (i = 0; i < 100000000; i++)
    {
    }

    printf("1: destroying tls\n");
    if (tls_destroy() == 0) printf("destroy succeeded\n");
    else printf("destroy failed\n");


    return NULL;
}

void* tls_tester1(void* arg)
{
    printf("creating thread 2\n");
    char buffer[1000];
    int i;

    printf("2: cloning first tls\n");
    if (tls_clone(140737345828608) == 0) printf("tls_clone succeeded\n");
    else printf("tls_clone failed\n");

    printf("2: reading from end of page\n");
    if (tls_read(4095, 3, buffer) == 0) printf("read succeeded: %s\n", buffer);
    else printf("read failed\n");

    printf("2: attempting to write to second page of TLS\n");
    if (tls_write(5000,5, "hello") == 0) printf("write succeeded\n");
    else printf("write failed\n");

    printf("2: reading from second page\n");
    if (tls_read(5000, 5, buffer) == 0) printf("read succeeded: %s\n", buffer);
    else printf("read failed\n");

    printf("2: attempting to write to first page of TLS\n");
    if (tls_write(2000,5, "world") == 0) printf("write succeeded\n");
    else printf("write failed\n");

    printf("2: reading from first page\n");
    if (tls_read(2000, 5, buffer) == 0) printf("read succeeded: %s\n", buffer);
    else printf("read failed\n");

    for (i = 0; i < 100000000; i++)
    {
    }

    printf("2: destroying tls\n");
    if (tls_destroy() == 0) printf("destroy succeeded\n");
    else printf("destroy failed\n");


    return NULL;
}

int main()
{
    int i = 0;
    pthread_t thread[2];

    pthread_create(&thread[0], NULL, tls_tester0, NULL);
    pthread_create(&thread[1], NULL, tls_tester1, NULL);

    for (i = 0; i < 1000000000; i++)
    {
    }

    return 0;
}

