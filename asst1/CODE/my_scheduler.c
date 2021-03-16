#include <assert.h>
#include "my_scheduler.h"

void insert_ready_q(tcb* thread){
  insert_tail(ready_q, thread);
}

void enter_scheduler(struct itimerspec* ovalue) {
  timer_settime(sig_timer,0,&timer_stopper,ovalue);
}

void exit_scheduler(struct itimerspec* ovalue) {
  timer_settime(sig_timer,0,ovalue,NULL);
}

// TODO test: is it faster to reset the quantum for each thread or allow
//  threads to run for multiple interrupt cycles?

void schedule(int sig, siginfo_t* info, void* ucontext) {
//  if (in_scheduler) {
//    should_swap = 1;
//    return;
//  }

  should_swap = 0;
  tcb* old_thread = (tcb*) delete_head(ready_q);
  ucontext_t* old_context = old_thread->context;
  if ( old_thread -> status == READY ) { //DONE or BLOCKED status, don't insert back to ready queue
  	insert_ready_q(old_thread); 
  }
  // TODO: if the status is BLOCKED, how do we maintain access to the tcb?
//  assert(0);

  exit_scheduler(&timer_pause_dump);
  swapcontext(old_context, ((tcb*)get_head(ready_q))->context);
}
