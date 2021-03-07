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

typedef uint my_pthread_t;

// TODO: Bharti
typedef struct _ll {
  void get_head();
  void get_tail();
  void delete_head();
  void insert_head(void* thing);
  void insert_tail(void* thing);
} linked_list_t;

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  /* add something here */
  // 0 free, 1 locked
  bool locked = 0;
  // 0 if not locked
  uint32_t owner = 0;
  // priority assigned to this mutex. Updated when any thread blocks on this mutex.
  int hoisted_priority = 127;
} my_pthread_mutex_t;

typedef struct threadControlBlock {
	/* add something here */
  uint32_t id;
  void* stack_ptr;
  ucontext_t context;
  char retval;

  // linked-list of threads waiting on this thread
  linked_list_t* waited_on;

  // lock it's waiting on right now
  my_pthread_mutex_t* waiting_on;

} tcb; 


// TODO: adapt to priority queue
/* define your data structures here: */
typedef linked_list_t ready_q_t;

// TODO: Bharti
typedef struct _hashmap {
  
  tcb* get(int id);
  void put(tcb* thing);
} hashmap;

// ready queue, will be inited when scheduler created
ready_q_t* ready = 0;

// if scheduler is running
bool in_scheduler = 0;

// set by signal interrupt if current context is 0 but a scheduled swap should occur
// will be set by the scheduler to 0 after each scheduling decision
bool should_swap = 0;

hashmap done;

/* Function Declarations: */

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
