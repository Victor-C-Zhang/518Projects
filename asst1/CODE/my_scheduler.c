#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "my_pthread_t.h"


void insert_ready_q(tcb* thread){ 
	insert_tail(ready_q, thread);
}

tcb* get_active_thread() {
	return ( (tcb*) get_head(ready_q) );
}

/**
 * Will ONLY be called via signal. (Set SA mask to ignore this signal)
 * Check in_scheduler. If 0, set should_swap and yield.
 * Saves context of currently running thread (?).
 * Moves thread to end of ready queue.
 * sets context to head of ready queue.
 */
void schedule(){	
	//SIGALRM to set timers!
}
