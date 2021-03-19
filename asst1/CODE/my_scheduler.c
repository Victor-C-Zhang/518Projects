#include <assert.h>
#include <math.h>
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
  old_thread->last_run = cycles_run;
  ++cycles_run;
  --should_maintain;

  if ( old_thread -> status == READY ) { //DONE or BLOCKED status, don't insert back to ready queue
    if (old_thread->cycles_left > 0) { // allow threads to run for
      // multiple interrupt cycles
      --(old_thread->cycles_left);
      insert_head(ready_q[curr_prio], old_thread);
      return;
    }
    if (old_thread->cycles_left == -1 || old_thread->acq_locks > 0) { // yield()
      // prio shouldn't change; hoisted prio shouldn't change
      old_thread->cycles_left = old_thread->priority;
      insert_ready_q(old_thread, old_thread->priority);
    } else if (curr_prio == NUM_QUEUES - 1) { // cannot increase
      old_thread->priority = NUM_QUEUES - 1;
      old_thread->cycles_left = NUM_QUEUES - 1;
      insert_ready_q(old_thread,old_thread->priority);
    } 
    else {
      old_thread->priority = old_thread->priority + 1;
      old_thread->cycles_left = old_thread->priority + 1;
      insert_ready_q(old_thread,old_thread->priority);
    }
  }

  if (should_maintain <= 0) {
    run_maintenance();
    should_maintain = ONE_SECOND/QUANTUM;
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
    exit_scheduler(&timer_25ms);
  }
  if (new_context == NULL) { // teardown and cleanup
    // TODO: free tcb contents: context, linkedlist
    free_map(all_threads);
    for (int i = 0; i < NUM_QUEUES; ++i) {
      free_list(ready_q[i]);
    }
    free(sig_timer);
    free(act);
    exit(0);
  }
  swapcontext(old_context, new_context);
}

/* Uses function
 * new_prio = floor{ (old_prio+1) e^{-x/g(old_prio+1)} }
 * g(y) = \frac{CYCLES_SINCE_LAST}{\ln y} - 1
 * where x \in [1,\infty) is the number of cycles since a thread was last run
 * and CYCLES_SINCE_LAST \geq ONE_SECOND/QUANTUM is the number of cycles
 * since the last maintenance cycle
 */
void run_maintenance() {
  for (int i = 1; i < NUM_QUEUES; ++i) {
    node_t* ptr = ready_q[i]->head;
    node_t* prev_ptr = NULL;
    while (ptr != NULL) {
      tcb* thread = (tcb*) ptr->data;
      int cycles_since_last = ONE_SECOND/QUANTUM - should_maintain;
      int x = (int)(cycles_run-thread->last_run);
      double gy = (cycles_since_last)/log(i+1) - 1;
      int new_prio = (int)((thread->priority + 1)*exp(-(double)x/gy));
      if (new_prio == thread->priority) { // do nothing
        prev_ptr = ptr;
        ptr = ptr->next;
      } else { // remove thread from current queue and add it to lower queue
        thread->priority = new_prio;
        insert_ready_q(thread, new_prio);
        if (ptr == ready_q[i]->head) {
          ready_q[i]->head = ptr->next;
        } else {
          prev_ptr->next = ptr->next;
        }
        if (ptr == ready_q[i]->tail) {
          ready_q[i]->tail = NULL;
        }
        ptr = ptr->next;
      }
    }
  }
}
