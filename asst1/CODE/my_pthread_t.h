// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdint.h>
#include "datastructs_t.h"

#define STACKSIZE 32768 

/***********************************
* STRUCT DEFINITIONS 
***********************************/
typedef uint my_pthread_t;

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  int locked; //0 FREE, 1 LOCKED
  uint32_t owner;
  int hoisted_priority; // priority assigned to this mutex. Updated when any thread blocks on this mutex.
} my_pthread_mutex_t;

/* tcb struct definition */
typedef struct threadControlBlock {
  int32_t id;
  void* stack_ptr;
  ucontext_t context;
  void* ret_val;
  linked_list_t* waited_on; // linked-list of threads waiting on this thread
  my_pthread_mutex_t* waiting_on; // lock it's waiting on right now
} tcb; 

/***********************************
* GLOBAL VARIABLES 
***********************************/
typedef linked_list_t ready_q_t; // TODO: adapt to priority queue
ready_q_t* ready_q; // ready queue, will be inited when scheduler created
tcb* scheduler_tcb;
int in_scheduler; // == 1 is true 
// set by signal interrupt if current context is 0 but a scheduled swap should occur
// will be set by the scheduler to 0 after each scheduling decision
int should_swap;
hashmap done; //threads that have completed 


/**********************************
 * FUNCTION DEFINITIONS 
***********************************/

/**
 * Returns currently running thread
 */
tcb* get_active_thread();

/**
* Insert function for ready queue
* update based on queue type, intially FIFIO, update later to priority
*/
void insert_ready_q(); 

/**
 * Will ONLY be called via signal. (Set SA mask to ignore this signal)
 * Check in_scheduler. If 0, set should_swap and yield.
 * Saves context of currently running thread (?).
 * Moves thread to end of ready queue.
 * sets context to head of ready queue.
 */
void scheduler();

/* create a new thread */
/**
 * If it's the first thing created (ready = 0), spin up a new scheduler.
 * Set tcb struct values, ucontext values.
 * Getcontext to save the current context.
 * Makecontext for new thread to run the (function).
 * Add to ready queue // TODO: with priority... (number).
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
