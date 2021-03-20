// File:  my_pthread_t.h
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include "datastructs_t.h"
#include "my_scheduler.h"

typedef uint my_pthread_t;

#define _GNU_SOURCE
#define pthread_create(a,b,c,d) my_pthread_create((my_pthread_t*)a,b,c,d)
#define pthread_yield() my_pthread_yield()
#define pthread_exit(x) my_pthread_exit(x)
#define pthread_join(x,y) my_pthread_join(x,y)
#define pthread_mutex_init(x,y) my_pthread_mutex_init(x,y)
#define pthread_mutex_lock(x) my_pthread_mutex_lock(x)
#define pthread_mutex_unlock(x) my_pthread_mutex_unlock(x)
#define pthread_mutex_destroy(x) my_pthread_mutex_destroy(x)


/***********************************
* STRUCT DEFINITIONS 
***********************************/

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  int locked; //0 FREE, 1 LOCKED
  uint32_t owner;
  int hoisted_priority; // priority assigned to this mutex. Updated when any thread blocks on this mutex.
  int holding_thread_priority; //priority of the thread which acquired lock
//  linked_list_t* waiting_on; // linked-list of threads waiting on this lock
} my_pthread_mutex_t;

/**********************************
 * FUNCTION DEFINITIONS 
***********************************/

/* create a new thread */
/**
 * If it's the first thing created (ready = 0), spin up a new scheduler.
 * Set tcb struct values, ucontext values.
 * Getcontext to save the current context.
 * Makecontext for new thread to run the (function).
 * Add to ready queue
 */
// Bharti
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
/**
 * Swap to scheduler.
 * Put thread at back of queue.
 * Put new head of ready queue to back of the queue and switch to that context.
 */
// Victor
int my_pthread_yield();

/* terminate a thread */
/**
 * Swap to scheduler.
 * Remove thread from queue and put into done hashmap.
 * Alert waited_on
 */
// Bharti
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
/**
 * Swap to scheduler.
 * Look for thread in done hashmap. If there, return return value.
 * Otherwise:
 *    remove thread from ready queue and add to waited_on queue.
 *    schedule next thread.
 */
// Bharti
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
/**
 * Create new mutex struct.
 */
// Victor
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
/**
 * test_and_set
 * If succeeds return.
 * If fails, yield to scheduler.
 * Update hoisted_priority
 */
// Victor
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
/**
 * Check ownership.
 * If owner, --locked;
 */
// Victor
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
/**
 * Lock the mutex, then free.
 */
// Victor
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif
