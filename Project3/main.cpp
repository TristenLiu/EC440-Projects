#include "threads.h"
#include <stdio.h>
#include <string.h>

#define NUM_THREADS 5

/*
	Copied from sample program
	Waste some time to visualize the running threads
*/
sem_t empty;
sem_t full;
sem_t mutex;
int in = 0;
int out = 0;
int buffer[10];

void producer()
{
	printf("producer starting for thread %d\n", (unsigned int)pthread_self());
	int item;
	int i = 0;
	while (i < 10)
	{
		item = i;
		//printf("decrementing empty in prod: %d\n", (unsigned int)pthread_self());
		sem_wait(&empty);
		//printf("decrementing mutex in prod: %d\n", (unsigned int)pthread_self());
		sem_wait(&mutex);
		buffer[in] = item;
		printf("producer %d: inserted %d in index %d\n", (unsigned int)pthread_self(), item, in);
		in = (in + 1) % 10;
		//printf("incrementing mutex in prod: %d\n", (unsigned int)pthread_self());
		sem_post(&mutex);
		//printf("incrementing full in prod: %d\n", (unsigned int)pthread_self());
		sem_post(&full);
		i++;
	}
}

void consumer()
{
	printf("consumer starting for thread %d\n", (unsigned int)pthread_self());
	int item;
	int i = 0;
	while (i < 10)
	{
		item = i;
		//printf("decrementing full in consumer: %d\n", (unsigned int)pthread_self());
		sem_wait(&full);
		//printf("decrementing mutex in consumer: %d\n", (unsigned int)pthread_self());
		sem_wait(&mutex);
		item = buffer[out];
		printf("consumer %d: removed %d in index %d\n", (unsigned int)pthread_self(), item, out);
		out = (out + 1) % 10;
		//printf("incrementing mutex in consumer: %d\n", (unsigned int)pthread_self());
		sem_post(&mutex);
		//printf("incrementing empty in consumer: %d\n", (unsigned int)pthread_self());
		sem_post(&empty);
		i++;
	}
}

int main()
{
	pthread_t producers[NUM_THREADS], consumers[NUM_THREADS];

	sem_init(&mutex, 0, 1);
	sem_init(&empty, 0, 10);
	sem_init(&full, 0, 0);

	int i = 0;
	for (i = 0; i < NUM_THREADS; i++)
	{
		printf("making producer thread %d\n", i);
		pthread_create(&producers[i], NULL, (void *)producer, NULL);
		i++;
	}
	i = 0;
	for (i = 0; i < NUM_THREADS; i++)
	{
		printf("making consumer thread %d\n", i);
		pthread_create(&consumers[i], NULL, (void *)consumer, NULL);
		i++;
	}

	i = 0;
	// main thread wait for all threads to exit
	for (i = 0; i < NUM_THREADS; i++)
	{
		pthread_join(producers[i], NULL);
		i++;
	}
	i = 0;
	for (i = 0; i < NUM_THREADS; i++)
	{
		pthread_join(consumers[i], NULL);
		i++;
	}

	printf("destroying semaphores\n");
	sem_destroy(&mutex);
	sem_destroy(&empty);
	sem_destroy(&full);

	return 0;
}