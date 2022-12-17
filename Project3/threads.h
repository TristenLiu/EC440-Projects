#ifndef THREAD_H
#define THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <unistd.h>
#include "semaphore.h"

// Manage the status of the thread, 0 = Ready, 1 = Running, 2 = Exited, 3 = New thread
enum Status
{
    READY,
    RUNNING,
    EXITED,
    WAITING,
    FRESH,
    BLOCKED
};

// Thread control block needs to have its ID, status, a pointer to its stack and registers
typedef struct 
{
    pthread_t id;
    enum Status status;
    void *stack;
    jmp_buf reg;
    void* exitcode;
    pthread_t joining;
} thread;

enum semStatus
{
    DESTROYED,
    INITIALIZED
};

typedef struct
{
    int val;
    pthread_t waiting[128];
    enum semStatus status;
} seminfo;


void scheduler();
/*
    If the current thread is running, change it to ready
    Save the current state if the thread has not exited
    Find the next ready thread and jump to it
*/

void init_system();
/*
    Initizalize the necessary system functions on first thread creation
    Set the state of all threads in TCB to be FRESH (unmodified)
    Set up ualarm and the signal handler to catch it
*/

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
/*
    Create a new thread context and set its status to READY
    Allocate its stack memory
    Initialize its registers to its function call + args
    >scheduler MAY choose to schedule upon creation of a new thread
*/

void pthread_exit(void *value_ptr);
/*
    Terminate the calling thread
    Change thread status to EXITED
    Free the stack
    If there are any more ready threads, call the scheduler
*/

void pthread_exit_wrapper();
/*
    Provided wrapper for pthread_exit to access the $rax register
*/

pthread_t pthread_self();
/*
    Return the Thread ID
*/

/*
    PROJECT 3
*/

void lock();
/*
    Lock the current running thread and prevent it from being interrupted
    Using sigprocmask
*/

void unlock();
/*
    Unlock the current running thread and allow SIGARLM to go through again
*/

int pthread_join(pthread_t thread, void **value_ptr);
/*
    Postpone the execution of the current running thread until the target thread exits
    Handle exit codes in some way
*/

int sem_init(sem_t *sem, int pshared, unsigned value);
/*
    Initialize the semaphore referred to by sem
*/

int sem_wait(sem_t *sem);
/*
    Decrement the semaphore pointed to by sem
    if sem > 0 : decrement and return
    if sem = 0 : block until it is possible to decrement
*/

int sem_post(sem_t *sem);
/*
    Increment the semaphore pointed to by sem
    if sem > 0 : wake up another thread thats waiting for it
*/

int sem_destroy(sem_t *sem);
/*
    Destroy the semaphore pointed to by sem

*/

#endif