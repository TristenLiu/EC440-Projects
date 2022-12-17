#include "threads.h"
#include <stdio.h>

#define NUM_THREADS 8

/* 
    Copied from sample program
    Waste some time to visualize the running threads
*/
void *count(void *arg) {
	unsigned long int c = (unsigned long int)arg;
	int i;
	for (i = 0; i < c; i++) {
		if ((i % 1000000) == 0) {
			
			printf("tid: 0x%x Just counted to %d of %ld\n", (unsigned int)pthread_self(), i, c);
			
		}
	}
    return arg;
}

int main()
{
	pthread_t threads[NUM_THREADS];
	int i;
	unsigned long int cnt = 10000000;

    //create THRAD_CNT threads
	for(i = 0; i<NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, count, (void *)((i+1)*cnt));
	}

    //join all threads ... not important for proj2
	//for(i = 0; i<THREAD_CNT; i++) {
	//	pthread_join(threads[i], NULL);
	//}
    // But we have to make sure that main does not return before 
    // the threads are done... so count some more...
    count((void *)(cnt*(NUM_THREADS + 2)));
	return 0;
}