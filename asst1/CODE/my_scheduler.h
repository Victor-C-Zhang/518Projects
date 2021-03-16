#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

#include <signal.h>
#include "datastructs_t.h"

#define STACKSIZE 32768
#define QUANTUM 25000000

typedef linked_list_t ready_q_t; // TODO: adapt to priority queue

typedef enum thread_status{READY, DONE, BLOCKED} thread_status;
/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  int locked; //0 FREE, 1 LOCKED
  uint32_t owner;
  int hoisted_priority; // priority assigned to this mutex. Updated when any thread blocks on this mutex.
} my_pthread_mutex_t;

/* tcb struct definition */
typedef struct threadControlBlock {
  uint32_t id;
  void* stack_ptr;
  ucontext_t* context;
  void* ret_val;
  thread_status status;
  linked_list_t* waited_on; // linked-list of threads waiting on this thread
  my_pthread_mutex_t* waiting_on; // lock it's waiting on right now
} tcb;


ready_q_t* ready_q; // ready queue, will be inited when scheduler created
int in_scheduler; // if scheduler is running
// set by signal interrupt if current context is 0 but a scheduled swap should occur
// will be set by the scheduler to 0 after each scheduling decision
int should_swap;
hashmap* all_threads; //all threads accessed by ids

/**
 * Will only be called via SIGALRM. (No need to set SA mask to ignore duplicate signal)
 * Check in_scheduler. If 1, set should_swap and yield.
 * Another instance of the scheduler should check should_swap to see if a swap is requested
 * and raise a signal, if necessary.
 * Saves context of currently running thread (?).
 * Moves thread to end of ready queue.
 * sets context to head of ready queue.
 */
void schedule(int sig, siginfo_t* info, void* ucontext);
void insert_ready_q(tcb* thread);

#endif
