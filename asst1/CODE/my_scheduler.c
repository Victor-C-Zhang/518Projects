#include "my_scheduler.h"

void schedule(int sig, siginfo_t* info, void* ucontext) {
  if (in_scheduler) {
    should_swap = 1;
    return;
  }
  should_swap = 0;
  ucontext_t* old_context = ((tcb*)ready->head->data)->context;
  insert_tail(ready, delete_head(ready));
//  printf("Swapping from scheduler\n");
  swapcontext(old_context, ((tcb*)ready->head->data)->context);
}