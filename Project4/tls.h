#ifndef TLS_H
#define TLS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>

typedef struct thread_local_storage
{
    pthread_t tid;
    unsigned int size; /* size in bytes */
    unsigned int page_num; /* number of pages */
    struct page **pages; /* array of pointers to pages */
} TLS;

typedef struct page {
    unsigned long int address; /* start address of page */
    int ref_count; /* counter for shared pages */
} page;

typedef struct hash_element
{
    pthread_t tid; /* tid for this hash element */
    TLS *tls; /* tls pointer */
    struct hash_element *next; /* next TLS in hash table index*/
} hash_element;

int tls_create(unsigned int size);
/*
    Create a local storage area for the running thread that can store at least size bytes
    return -1 on error and 0 on success
*/

int tls_write(unsigned int offset, unsigned int length, char *buffer);
/*
    Read length bytes from the buffer, and write it into the LSA of the currently running thread
    The information should be written starting at position offset
    return -1 on error and 0 on success
*/

int tls_read(unsigned int offset, unsigned int length, char *buffer);
/*
    Read length bytes starting from the offset in the LSA, and write it into the buffer
    return -1 on error and 0 on success
*/

int tls_destroy();
/*
    Destroy the LSA of the currently running thread
    return -1 on error and 0 on success
*/

int tls_clone(pthread_t tid);
/*
    Clone the LSA of thread tid into the currently running thread
    Using COW semantics, only copy a specific page from the cloned LSA when writing to it.
    If a thread attempts to access another thread's LSA, then call pthread_exit
*/


#endif