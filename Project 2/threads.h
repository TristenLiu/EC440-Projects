#ifndef THREAD_H
#define THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <unistd.h>

// Manage the status of the thread, 0 = Ready, 1 = Running, 2 = Exited, 3 = New thread
enum Status
{
    READY,
    RUNNING,
    EXITED,
    FRESH
};

// Thread control block needs to have its ID, status, a pointer to its stack and registers
typedef struct 
{
    pthread_t id;
    enum Status status;
    void *stack;
    jmp_buf reg;
} thread;

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

pthread_t pthread_self();
/*
    Return the Thread ID
*/

#endif