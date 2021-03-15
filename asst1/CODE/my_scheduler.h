#include "my_pthread_t.h"

#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

#define MY_STACK_SIZE 16384
#define QUANTUM 25000000

extern ready_q_t* ready;

// if scheduler is running
int in_scheduler;// = 0;

// set by signal interrupt if in_scheduler is true but a scheduled swap should occur
// will be set by the scheduler to 0 after each scheduling decision
int should_swap;// = 0;

extern hashmap done;

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

#endif
