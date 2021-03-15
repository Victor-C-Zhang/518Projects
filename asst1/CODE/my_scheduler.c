#include "my_scheduler.h"

void insert_ready_q(tcb* thread){
  insert_tail(ready_q, thread);
}

tcb* get_active_thread() {
  return ( (tcb*) get_head(ready_q) );
}

void schedule(int sig, siginfo_t* info, void* ucontext) {
  if (in_scheduler) {
    should_swap = 1;
    return;
  }
  should_swap = 0;
  ucontext_t* old_context = ((tcb*)ready_q->head->data)->context;
  insert_tail(ready_q, delete_head(ready_q));
//  printf("Swapping from scheduler\n");
  swapcontext(old_context, ((tcb*)ready_q->head->data)->context);
}
