#include "my_pthread_t.h"

#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

/** might not need this header file....??? */


extern ready_q_t* ready;

extern bool in_scheduler;

extern bool should_swap;

extern hashmap done;

/**
 * Will ONLY be called via signal. (Set SA mask to ignore this signal)
 * Check in_scheduler. If 0, set should_swap and yield.
 * Saves context of currently running thread (?).
 * Moves thread to end of ready queue.
 * sets context to head of ready queue.
 */
void schedule();

#endif
