// File:  my_pthread_t.h
// Author:  Yujie REN
// Date:  09/23/2017

// name: Bharti Mehta, Victor Zhang
// username of iLab: bam270
// iLab Server: pwd
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
#include <signal.h>
#include <time.h>

/***********************************
* STRUCT DEFINITIONS 
***********************************/
typedef pid_t my_pthread_t;

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  int locked; //0 FREE, 1 LOCKED
  my_pthread_t owner;
  int hoisted_priority; // priority assigned to this mutex. Updated when any thread blocks on this mutex.
  int holding_thread_priority; //priority of the thread which acquired lock
} my_pthread_mutex_t;

#define pthread_mutex_t my_pthread_mutex_t
#define pthread_attr_t void*
#define pthread_mutexattr_t void*
#define pthread_create(a,b,c,d) my_pthread_create((my_pthread_t*)a,b,c,d)
#define pthread_yield() my_pthread_yield()
#define pthread_exit(x) my_pthread_exit(x)
#define pthread_join(x,y) my_pthread_join(x,y)
#define pthread_mutex_init(x,y) my_pthread_mutex_init(x,y)
#define pthread_mutex_lock(x) my_pthread_mutex_lock(x)
#define pthread_mutex_unlock(x) my_pthread_mutex_unlock(x)
#define pthread_mutex_destroy(x) my_pthread_mutex_destroy(x)

/**********************************
 * FUNCTION DEFINITIONS 
***********************************/

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif
