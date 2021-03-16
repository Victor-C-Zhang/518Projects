#include <assert.h>
#include "my_scheduler.h"

void insert_ready_q(tcb* thread, int queue_num){
  assert(queue_num < NUM_QUEUES);
  insert_tail(ready_q[queue_num], thread);
}

void enter_scheduler(struct itimerspec* ovalue) {
  timer_settime(sig_timer,0,&timer_stopper,ovalue);
  in_scheduler = 1;
}

void exit_scheduler(struct itimerspec* ovalue) {
  timer_settime(sig_timer,0,ovalue,NULL);
}

// TODO test: is it faster to reset the quantum for each thread or allow
//  threads to run for multiple interrupt cycles?

void schedule(int sig, siginfo_t* info, void* ucontext) {
  tcb* old_thread = (tcb*) delete_head(ready_q[curr_prio]);
  ucontext_t* old_context = old_thread->context;
  if ( old_thread -> status == READY ) { //DONE or BLOCKED status, don't insert back to ready queue
  	if (old_thread->priority == -1) { // yield() doesn't impact priority
  	  old_thread->priority = curr_prio;
  	  insert_ready_q(old_thread, curr_prio);
  	} else if (old_thread->priority == NUM_QUEUES - 1) { // cannot increase
  	  insert_ready_q(old_thread,curr_prio);
  	} else {
  	  ++(old_thread->priority);
  	  insert_ready_q(old_thread,curr_prio+1);
  	}
  }

  struct ucontext_t* new_context = NULL;
  // pick highest priority task to run
  for (int i = 0; i < NUM_QUEUES; ++i) {
    if (!isEmpty(ready_q[i])) {
      curr_prio = i;
      new_context = ((tcb*)get_head(ready_q[i]))->context;
      break;
    }
  }
  if (in_scheduler) {
    in_scheduler = 0;
    exit_scheduler(&timer_pause_dump);
  }
  if (new_context == NULL) {
    exit(0); // TODO: teardown and cleanup
  }
  swapcontext(old_context, new_context);
}
