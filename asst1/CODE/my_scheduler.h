#include "my_pthread_t.h"

#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

extern ready_q_t* ready;

extern int in_scheduler;

extern int should_swap;

extern hashmap done;

/**
 * Will ONLY be called via SIGALRM. (No need to set SA mask to ignore duplicate signal)
 * Check in_scheduler. If 0, set should_swap and yield.
 * Saves context of currently running thread (?).
 * Moves thread to end of ready queue.
 * sets context to head of ready queue.
 */
void schedule(int sig, siginfo_t* info, void* ucontext);

#endif
