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

 // TODO: if the status is BLOCKED, how do we maintain access to the tcb?
  should_swap = 0;
  tcb* old_thread = (tcb*) delete_head(ready_q);
  ucontext_t* old_context = old_thread->context;
  if ( old_thread -> status == READY ) { //DONE or BLOCKED status, don't insert back to ready queue
  	insert_ready_q(old_thread); 
  }
  tcb* new_thread = (tcb*) get_head(ready_q);
  while (new_thread->waiting_on != NULL){
	if (new_thread->waiting_on->locked == 0){
  		new_thread->waiting_on->locked = 1;
		new_thread->waiting_on->owner = new_thread->id;
		new_thread -> waiting_on = NULL;
		printf("thread %d got lock \n", new_thread->id);
		break;        
	}
    	insert_tail(ready_q, delete_head(ready_q));
	new_thread = (tcb*) get_head(ready_q);
  }
  
  exit_scheduler(&timer_pause_dump);
  swapcontext(old_context, new_thread->context);
}
