/*
 * Name: Justin Sadler
 * Date: 2022-11-16
 */

/*
  This source code file runs producer and consumer program. Requires a "tls.h" header file in the same directory that contains TLS
  function declarations. 
  Compile this source code file with "-lpthread" or with your own pthread library (if you're brave). 

  Producer consumer code followed example here:  
  https://shivammitra.com/c/producer-consumer-problem-in-c/#
*/

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "tls.h"

#define SIZE 50
#define WAIT 1000000 // wait time multiplier, DON'T CHANGE THIS 
#define MAX_SEC   3  // Max wait time in seconds


// NOTE: By default, wait time will be between 1 and MAX_SEC seconds

sem_t mutex;
sem_t empty;
sem_t full;

int in = 0; // Index at which producer will put next data
int out = 0; // Index where consumer will consume data
int buffer[SIZE];

void printArr() {

    printf("[");
    size_t i;
    for(i = 0; i < SIZE; i++) {
        printf("%d,", buffer[i]);
    }
    printf("]\n");
}

void * producer(void * arg) {
    printf("Starting producer\n");
    int item = 1;
    assert(tls_read(1, 1, NULL) == -1);
    while(1) {
        int wait_time = ((rand() % MAX_SEC) + 1) * WAIT;
        printf("Producer waiting %d s\n", wait_time / 1000000);
        usleep(wait_time); // waste time
        item++; // produce item
        printf("Producer: %d\n", item);
        sem_wait(&empty);
        sem_wait(&mutex);
        // insert item
        buffer[in] = item;
        printArr();
        in = (in + 1) % SIZE;
        sem_post(&mutex);
        sem_post(&full);
    }
    return 0;
}

void * consumer(void * arg) {

    printf("Starting consumer\n");
    while(1) {
        int wait_time = ((rand() % MAX_SEC) + 1) * WAIT;
        printf("Consumer waiting %d seconds\n", wait_time / 1000000);
        usleep(wait_time); // waste time
        sem_wait(&full);
        sem_wait(&mutex);
        int item = buffer[out];
        buffer[out] = 0;
        printArr();
        out = (out + 1) % SIZE;
        sem_post(&mutex);
        sem_post(&empty);
        printf("Consumer: %d\n", item);
    }
    return 0;
}

int main(int argc, char ** argv) {

    srand(time(NULL));
    sem_init(&mutex, 0, 1);
    sem_init(&full, 0,  0);
    sem_init(&empty,0,  SIZE);

    pthread_t threads[2];

    pthread_create(&(threads[0]), NULL, producer, (void*)0);
    pthread_create(&(threads[1]), NULL, consumer, (void*)0);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    sem_destroy(&mutex);
    sem_destroy(&full);
    sem_destroy(&empty);

    return 0;
}