#include "my_scheduler.h"

void insert_ready_q(tcb* thread){
  insert_tail(ready_q, thread);
}

void schedule(int sig, siginfo_t* info, void* ucontext) {
  if (in_scheduler) {
    should_swap = 1;
    return;
  }
 
  should_swap = 0;
  tcb* old_thread = (tcb*) delete_head(ready_q);
  ucontext_t* old_context = old_thread->context;
  if ( old_thread -> status == READY ) { //DONE or BLOCKED status, don't insert back to ready queue
  	insert_ready_q(old_thread); 
  }

  swapcontext(old_context, ((tcb*)get_head(ready_q))->context);
}
