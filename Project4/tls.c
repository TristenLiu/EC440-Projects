#define _XOPEN_SOURCE 700
#define _BSD_SOURCE

#include "tls.h"

#define MAX_THREADS 128
#define HASH_SIZE 31

static int initialized = 0;
static int page_size;

static struct sigaction sigact;
static hash_element *hash_table[HASH_SIZE];

/* Helper Functions */
static void tls_unprotect(page *p)
{
    if (mprotect((void *)p->address, page_size, PROT_READ | PROT_WRITE))
    {
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        perror("tls_unprotect failed");
        exit(1);
    }
}

static void tls_protect(page *p)
{
    if (mprotect((void *)p->address, page_size, 0))
    {
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

hash_element *hash_search(pthread_t tid)
{
    /* division hash function */
    int index = tid % HASH_SIZE;

    if (hash_table[index] != NULL)
    {
        hash_element *iter = hash_table[index];
        /* loop through the hash table index until end of index or found tid*/
        while (iter != NULL)
        {
            if (iter->tid == tid)
                return iter;
            else
                iter = iter->next;
        }
    }
    /* if the tid was not found, return NULL */
    return NULL;
}

static void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
    /* get starting page address of fail */
    int p_fault = ((unsigned long int)si->si_addr) & ~(page_size - 1);

    /* check if fail happened inside of a TLS */
    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        hash_element *element = hash_table[i];

        /* check all elements of the linked list */
        while (element != NULL)
        {
            int j;
            TLS *working_tls = element->tls;
            /* check all pages of the tls */
            for (j = 0; j < working_tls->page_num; j++)
            {
                /* if the address of the fault equals the page of an address, pthread_exit*/
                if (p_fault == working_tls->pages[j]->address)
                    pthread_exit(NULL);
            }
            element = element->next;
        }
    }

    /* if the segfault did not happen at any of the pages, call default handler */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    raise(sig);
}

static void tls_init()
{
    initialized = 1;

    page_size = getpagesize();
    /* get the size of a page */
    /* install the signal handler for page faults (SIGSEGV, SIGBUS) */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO; /* use extended signal handling */
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);
}

int tls_create(unsigned int size)
{
    /* initialize the the system on first call */
    if (!initialized)
        tls_init();

    /* If the size is less than 0, return an error */
    /* If the TLS for the current thread is already created, return an error */
    pthread_t tid = pthread_self();
    if (size <= 0 || hash_search(tid) != NULL)
        return -1;

    /* if no errors, create the TLS */
    TLS *tls = (TLS *)malloc(sizeof(TLS));
    tls->tid = tid;
    tls->size = size;
    /* if size is greater than a multiple of page_size, then allocate an extra page */
    tls->page_num = (size / page_size) + (size % page_size > 0);

    /* allocate all the necessary pages */
    tls->pages = (page **)calloc(tls->page_num, sizeof(page *));
    int i;
    for (i = 0; i < tls->page_num; i++)
    {
        page *tmp_page = (page *)malloc(sizeof(page));
        tmp_page->address = (unsigned long int)mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0);
        /* check for successful map */
        if (tmp_page->address == (unsigned long int)MAP_FAILED)
            return -1;
        tmp_page->ref_count = 1;
        tls->pages[i] = tmp_page;
    }

    /* add the new TLS to the hash table */
    int index = tid % HASH_SIZE;
    hash_element *insert = (hash_element *)malloc(sizeof(hash_element));
    insert->tid = tid;
    insert->tls = tls;

    /* if there is an existing index, insert to head of linked list */
    if (hash_table[index] != NULL)
        insert->next = hash_table[index];
    else
        insert->next = NULL;

    hash_table[index] = insert;

    //printf("tid is: %lu\n", (unsigned long)pthread_self());

    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t tid = pthread_self();
    hash_element *element = hash_search(tid);
    /* ERROR CHECKING */
    /* check if the tls exists in the hash table */
    if (element == NULL)
        return -1;
    /* check if the write length is within the allocated size */
    TLS *working_tls = element->tls;
    if (offset + length > working_tls->size)
        return -1;

    /* unprotect all page in the tls */
    int i;
    for (i = 0; i < working_tls->page_num; i++)
        tls_unprotect(working_tls->pages[i]);

    /* write to the pages */
    int indx;
    for (i = 0, indx = offset; indx < (offset + length); i++, indx++)
    {
        page *main_page;
        unsigned int working_page, page_offset;
        /* working_page tells us which page number to write to */
        working_page = indx / page_size;
        /* page_offset tells us at what position on the page to write to */
        page_offset = indx % page_size;
        main_page = working_tls->pages[working_page];

        /* check if there are multiple references to the page */
        if (main_page->ref_count > 1)
        {
            /* create a private copy for the working tls */
            page *copy = (page *)malloc(sizeof(page));
            copy->address = (unsigned long int)mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
            /* copy the main page to the new page */
            memcpy((void *)copy->address, (void *)main_page->address, page_size);
            copy->ref_count = 1;
            working_tls->pages[working_page] = copy;

            /* update the main page */
            main_page->ref_count--;
            tls_protect(main_page);
            main_page = copy;
        }
        char *dst = (char *)(main_page->address + page_offset);
        *dst = buffer[i];
    }

    /* reprotect the pages after writing */
    for (i = 0; i < working_tls->page_num; i++)
        tls_protect(working_tls->pages[i]);

    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t tid = pthread_self();
    hash_element *element = hash_search(tid);
    /* ERROR CHECKING */
    /* check if the tls exists in the hash table */
    if (element == NULL)
        return -1;
    /* check if the read length is within the allocated size */
    TLS *working_tls = element->tls;
    if (offset + length > working_tls->size)
        return -1;

    /* unprotect the page that is being read from */
    int i;
    for (i = 0; i < working_tls->page_num; i++)
        tls_unprotect(working_tls->pages[i]);

    /* read from the page into the buffer */
    int indx;
    for (i = 0, indx = offset; indx < (offset + length); i++, indx++)
    {
        page *main_page;
        unsigned int working_page, page_offset;

        working_page = indx / page_size;
        page_offset = indx % page_size;

        main_page = working_tls->pages[working_page];
        char *src = (char *)(main_page->address + page_offset);
        buffer[i] = *src;
    }

    /* reprotect the pages after reading */
    for (i = 0; i < working_tls->page_num; i++)
        tls_protect(working_tls->pages[i]);

    return 0;
}

int tls_destroy()
{
    /* Check if the thread has a TLS */
    pthread_t tid = pthread_self();
    hash_element *element = hash_search(tid);
    if (element == NULL)
        return -1;

    TLS *working_tls = element->tls;
    int i = 0;
    for (i = 0; i < working_tls->page_num; i++)
    {
        /* if the page is not being shared, then free the physical page and the struct page */
        if (working_tls->pages[i]->ref_count == 1)
        {
            munmap((void *)working_tls->pages[i]->address, page_size);
            free(working_tls->pages[i]);
        }
        /* if the page is being shared, then just decrement ref count */
        else if (working_tls->pages[i]->ref_count > 1)
            working_tls->pages[i]->ref_count--;
    }

    /* clean up the TLS */
    free(working_tls->pages);

    /* delete the TLS from the hash table */
    int indx = tid % HASH_SIZE;
    /* find the element before the deleted element */
    hash_element *temp = hash_table[indx];
    if (temp->tid != element->tid)
    {
        while (temp->next->tid != element->tid)
            temp = temp->next;
        temp->next = element->next;
    }
    free(working_tls);
    free(element);

    return 0;
}

int tls_clone(pthread_t tid)
{
    pthread_t dst_tid = pthread_self();
    hash_element *dst_element = hash_search(dst_tid);
    hash_element *src_element = hash_search(tid);

    /* The destination cannot have a TLS and the source must have a TLS */
    if (dst_element != NULL || src_element == NULL)
        return -1;

    /* clone  the TLS */
    TLS *dst_tls = (TLS *)malloc(sizeof(TLS));
    TLS *src_tls = src_element->tls;

    dst_tls->tid = dst_tid;
    dst_tls->size = src_tls->size;
    dst_tls->page_num = src_tls->page_num;

    /* allocate all the necessary pages */
    dst_tls->pages = (page **)calloc(dst_tls->page_num, sizeof(page *));
    int i;
    for (i = 0; i < src_tls->page_num; i++)
    {
        dst_tls->pages[i] = src_tls->pages[i];
        dst_tls->pages[i]->ref_count++;
    }

    /* create the hash element and add to the hash table*/
    dst_element = (hash_element *)malloc(sizeof(hash_element));
    dst_element->tid = dst_tid;
    dst_element->tls = dst_tls;

    int indx = dst_tid % HASH_SIZE;
    if (hash_table[indx] != NULL)
        dst_element->next = hash_table[indx];
    else
        dst_element->next = NULL;

    hash_table[indx] = dst_element;

    return 0;
}
