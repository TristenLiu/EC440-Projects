#define _XOPEN_SOURCE 700
#define _BSD_SOURCE

#include "threads.h"
#include "ec440threads.h"

// Maximum number of threads running at a time
#define MAX_THREADS 128
// Size of the stack allocated per thread
#define STACK_SIZE 32767

// Array index definitions for __jmp_buf
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

// Each of these should be active for all calls to this file
// Signal handler for SIGALRM
static struct sigaction alrm_handler;
// Save the current TID in a global var, initialize to 0 (main)
static pthread_t curr_TID = 0;
// Keep track of all the created threads
static thread TCB[MAX_THREADS];
// Keep track of the total number of threads
static int total_threads = 0;
// Do something special in the first call
static int first_call = 1;

void scheduler()
{
    // Find the next thread that is ready, avoid an infinite loop if there are no more READY threads
    pthread_t next = curr_TID + 1;
    while (TCB[next].status != READY && next != curr_TID)
        next = (next + 1) % MAX_THREADS;

    int jumped = 0;

    // Change the status of the currently running thread to READY and save the state
    if (TCB[curr_TID].status == RUNNING)
    {
        TCB[curr_TID].status = READY;
        jumped = setjmp(TCB[curr_TID].reg);
    }

    // setjmp returns 0 if returning directly, and nonzero when returning from longjmp
    // if we're just returning from a longjmp, don't longjmp again
    if (!jumped)
    {
        // Update the current running thread
        curr_TID = next;
        TCB[curr_TID].status = RUNNING;

        // Return 1 to the setjmp that its calling back to
        longjmp(TCB[next].reg, 1);
    }
}

void init_system()
{
    // initialize all TCB states to FRESH and set their id to their array index on first call
    pthread_t i = 0;
    while (i < MAX_THREADS)
    {
        TCB[i].status = FRESH;
        TCB[i].id = i;
        TCB[i].joining = -1;
        i++;
    }

    // send a SIGALRM after 50ms and then every 50ms after that (__useconds_t stores MICROseconds)
    __useconds_t cooldown = (50 * 1000);
    ualarm(cooldown, cooldown);

    // SIGALARM handler
    // When the alarm handler is triggered, call scheduler
    alrm_handler.sa_handler = &scheduler;
    // SA_NODEFER: Do not add the signal to the thread's signal mask while the handler is executing
    alrm_handler.sa_flags = SA_NODEFER;
    // When SIGALRM is caught, trigger the alarm handler
    sigaction(SIGALRM, &alrm_handler, NULL);
}

int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg)
{
    lock();
    // On the first call initialize the TCB and SIGALRM, and hold onto the main thread
    int main_thread = 0;
    if (first_call)
    {
        // printf("initializing system\n");
        init_system();
        first_call = 0;
        TCB[0].status = RUNNING;
        total_threads++;

        // Main thread jumps here
        main_thread = setjmp(TCB[0].reg);
    }

    // When main_thread jumps back, long_jmp will set it to 1
    if (!main_thread)
    {
        // If there are already a MAX amount of threads, return
        if (total_threads > MAX_THREADS)
        {
            printf("Error: Maximum Thread amount reached\n");
            return -1;
        }

        // Check for the closest FRESHthread without going out of bounds
        pthread_t i = 0;
        while (i < MAX_THREADS && TCB[i].status != FRESH)
        {
            i++;
        }
        // set first jump
        setjmp(TCB[i].reg);

        // Set input thread to the id of TCB
        *thread = i;
        // jmpbug stores long int types, addresses are unsigned
        // Set args (could be signed or unsigned) to R13 in jmpbuf
        TCB[i].reg->__jmpbuf[JB_R13] = (long int)arg;
        // Set start_routine to R12 in jmpbuf
        TCB[i].reg->__jmpbuf[JB_R12] = (unsigned long int)start_routine;
        // Set the program counter (RIP) to start_thunk
        TCB[i].reg->__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);

        /*
            Before we set the stack pointer in the RSP register, we need to put
            pthread_exit at the beginning of the stack, so a thread automatically
            exits at the end of its runtime
        */

        // Allocate the stack
        TCB[i].stack = malloc(STACK_SIZE);

        // The "top" of the stack starts at 32767
        // Allocate enough space for the pthread_exit function, function address is 8bytes long
        void *topspace = (TCB[i].stack + STACK_SIZE) - 8;
        void *exit_addr = (void *)&pthread_exit_wrapper;

        // Copy the address into the stack
        memcpy(topspace, &exit_addr, 8);

        // Set the stack pointer (RSP) to the start of the stack after the address of pthread_exit
        TCB[i].reg->__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)topspace);

        // After the thread is setup, it is ready to run
        TCB[i].status = READY;
        total_threads++;

        // Optional: Choose whether or not to run the scheduler after a new thread is created
        // scheduler();
    }
    // On main thread, main is stalling and waiting for the rest of the threads to finish

    unlock();
    // On successful call, reach the end and return 0
    return 0;
}

void pthread_exit(void *value_ptr)
{
    lock();
    // Change the status of the thread to waiting to be joined
    //("pthread_exit called on thread %d\n", (int)curr_TID);
    TCB[curr_TID].status = WAITING;

    int joined;

    if (TCB[curr_TID].joining != -1)
        // there is a thread waiting to join this thread
        joined = TCB[curr_TID].joining + MAX_THREADS;
    else
        joined = (int)setjmp(TCB[curr_TID].reg);

    if (!joined)
    {
        // Check if there are any more threads left to schedule (except the main thread)
        int i = 1;
        while (i < MAX_THREADS && TCB[i].status != READY)
            i++;

        // if there are no ready threads, jump back to main to finish main thread
        if (i == MAX_THREADS)
        {
            unlock();
            curr_TID = 0;
            longjmp(TCB[0].reg, 1);
        }

        // Call scheduler if there are still threads ready to be scheduled
        else
        {
            unlock();
            scheduler();
        }
    }
    else if (joined)
    {
        // If the thread has been joined, free the stack and jump back to the joining thread
        // printf("exiting thread %d after joined by thread %d\n", (int)curr_TID, joined);
        if (TCB[curr_TID].status != EXITED)
        {
            TCB[curr_TID].status = EXITED;
            // printf("freeing stack of thread %d\n", (int)curr_TID);
            free(TCB[curr_TID].stack);
            total_threads--;
        }
        // Copy the exit value of pthread_exit for pthread_join
        TCB[curr_TID].exitcode = (void *)value_ptr;
        // Remove offset
        curr_TID = (pthread_t)(joined - MAX_THREADS);

        // printf("jumping to thread %d\n", (int)curr_TID);
        unlock();
        longjmp(TCB[curr_TID].reg, 1);
    }
    // else in the main thread, exit 0
    exit(0);
}

void pthread_exit_wrapper()
{
    unsigned long int res;
    asm("movq %%rax, %0\n"
        : "=r"(res));
    pthread_exit((void *)res);
}

pthread_t pthread_self()
{
    return curr_TID;
}

void lock()
{
    // Block any incoming SIGALRMs
    sigset_t set, oset;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, &oset);
}

void unlock()
{
    // Unblock any incoming SIGALRMS
    sigset_t set, oset;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, &oset);
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    lock();
    if (TCB[thread].status != EXITED)
    {
        // printf("thread %d joining thread %d\n", (unsigned int)curr_TID, (unsigned int)thread);
        TCB[curr_TID].status = BLOCKED;
        int joined;
        joined = (int)setjmp(TCB[curr_TID].reg);

        // If the thread is waiting to exit, jump to it
        if (TCB[thread].status == WAITING && !joined)
        {
            pthread_t waiting_thread = curr_TID;
            curr_TID = thread;
            // printf("jumping to thread %d with arg %d\n", (int)thread, (int)waiting_thread);

            unlock();
            // If joining from main, longjmp cannot return a value of 0. Add an offset
            longjmp(TCB[thread].reg, (int)waiting_thread + MAX_THREADS);
        }
        // If the thread is not waiting
        else
        {
            // if the thread has not jumped back from pthread_exit, schedule
            if (!joined)
            {
                // The result of multiple calls to the same target is undefined
                TCB[thread].joining = curr_TID;
                unlock();
                scheduler();
            }
            // if the thread jumped back, its ready to be run again
            else
            {
                if (value_ptr != NULL)
                {
                    // copy the value from exit to the local return ptr
                    *value_ptr = (void *) TCB[thread].exitcode;
                }
                TCB[curr_TID].status = RUNNING;
                // printf("TID %d running \n", (int)curr_TID);
            }
        }
    }

    unlock();
    // On Success
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
    seminfo *SEB = malloc(sizeof(*SEB));
    SEB->val = value;
    SEB->status = INITIALIZED;

    int i = 0;
    while (i < MAX_THREADS)
    {
        SEB->waiting[i] = 0;
        i++;
    }

    sem->__align = (long int)SEB;
    return 0;
}

int sem_wait(sem_t *sem)
{
    seminfo *temp = (seminfo *)sem->__align;

    if (temp->status == INITIALIZED)
    {
        if (temp->val <= 0)
        {
            TCB[curr_TID].status = BLOCKED;
            int i = 0;
            // find the next open slot in the queue
            while (temp->waiting[i] != 0)
            {
                i++;
            }
            temp->waiting[i] = curr_TID;
            scheduler();
        }
        else if (temp->val > 0)
        {
            temp->val--;
        }
    }
    else
    {
        printf("ERROR: This semaphore is destroyed\n");
        return -1;
    }
    return 0;
}

int sem_post(sem_t *sem)
{
    seminfo *temp = (seminfo *)sem->__align;

    if (temp->status == INITIALIZED)
    {
        if (temp->val == 0)
        {
            // If the semaphore value is at 0, find the next waiting thread
            int i = 0;
            while (temp->waiting[i] == 0 && i < MAX_THREADS)
            {
                i++;
            }

            // If there are no waiting threads, increment the semaphore value
            if (i == MAX_THREADS)
            {
                temp->val++;
            }
            // If there was a waiting thread, set it to RUNNING and then jump to it
            else
            {
                pthread_t next_run = temp->waiting[i];
                temp->waiting[i] = 0;
                TCB[next_run].status = RUNNING;
                longjmp(TCB[next_run].reg, 1);
            }
        }
        else if (temp->val > 0)
        {
            temp->val++;
        }
    }
    else
    {
        printf("ERROR: This semaphore is destroyed\n");
        return -1;
    }

    return 0;
}

int sem_destroy(sem_t *sem)
{
    seminfo *temp = (seminfo *)sem->__align;

    if (temp->status == INITIALIZED)
    {
        temp->status = DESTROYED;
        free(temp);
    }
    return 0;
}
