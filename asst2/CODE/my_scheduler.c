#include <assert.h>
#include <math.h>
#include <sys/mman.h>
#include "my_scheduler.h"
#include "my_malloc.h"
#include "global_vals.h"
#include "direct_mapping.h"
#include "memory_finder.h"

void insert_ready_q(tcb* thread, int queue_num) {
  assert(queue_num < NUM_QUEUES);
  insert_tail(ready_q[queue_num], thread);
}

void enter_scheduler(struct itimerspec* ovalue) {
  timer_settime(&sig_timer,0,&timer_stopper,ovalue);
  in_scheduler = 1;
}

void exit_scheduler(struct itimerspec* ovalue) {
  for (unsigned i = stack_page_size; i < num_pages; ++i) {
    if (((pagedata*)myblock)[i].pid == 0)
      mprotect(mem_space + page_size*i, page_size, PROT_NONE);
  }
  timer_settime(&sig_timer,0,ovalue,NULL);
}

void alrm_handler(int sig, siginfo_t* info, void* ucontext) {
  enter_mem_manager(0);
  tcb* old_thread = (tcb*) ready_q[curr_prio]->head->data;
  swapcontext(old_thread->context, scheduler_context);
}

void schedule() {
  START_SCHED:
  enter_mem_manager(0);
  if (prev_done != NULL) {
    mydeallocate(prev_done, __FILE__, __LINE__, STACKDESREQ);
    prev_done = NULL;
  }
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
      swapcontext(scheduler_context, old_thread->context);
      goto START_SCHED;
    }
    if (old_thread->cycles_left == -1 || old_thread->acq_locks > 0) { // yield()
      // prio shouldn't change; hoisted prio shouldn't change
      old_thread->cycles_left = old_thread->priority;
    } else if (curr_prio == NUM_QUEUES - 1) { // cannot increase past max
      old_thread->priority = NUM_QUEUES - 1;
      old_thread->cycles_left = NUM_QUEUES - 1;
    } else { // age
      old_thread->priority = old_thread->priority + 1;
      old_thread->cycles_left = old_thread->priority + 1;
    }
    insert_ready_q(old_thread, old_thread->priority);

  } else if (old_thread->status == DONE && old_thread->id != 2) {
    prev_done = old_context->uc_stack.ss_sp;
    prev_done_id = old_thread->id;
  }

  if (should_maintain <= 0) {
    run_maintenance();
    should_maintain = ONE_SECOND/QUANTUM;
  }

  struct ucontext_t* new_context = NULL;
  my_pthread_t new_id = -1;
  // pick highest priority task to run
  for (int i = 0; i < NUM_QUEUES; ++i) {
    if (!isEmpty(ready_q[i])) {
      curr_prio = i;
      new_context = ((tcb*)get_head(ready_q[i]))->context;
      new_id = ((tcb *) get_head(ready_q[i]))->id;
      break;
    }
  }
  if (in_scheduler) {
    in_scheduler = 0;
    exit_scheduler(&timer_25ms);
  }
  if (new_context == NULL) { // teardown and cleanup
    exit(0);
  }

  // protect all data pages and swap stack of new thread
  mprotect(mem_space, page_size*num_pages, PROT_NONE);
  swap_stack(new_id);
  exit_mem_manager(new_id);
  swapcontext(scheduler_context, new_context);
  goto START_SCHED;
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
      new_prio = (new_prio > thread->priority) ?
            thread->priority : new_prio; // handle rare floating point rounding error
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
          ready_q[i]->tail = prev_ptr;
        }
        node_t* temp = ptr;
        ptr = ptr->next;
        mydeallocate(temp, __FILE__, __LINE__, LIBRARYREQ);
      }
    }
  }
}

void free_data() {
  if (prev_done != NULL) {
    mydeallocate(prev_done, __FILE__, __LINE__, STACKDESREQ);
  }
  enter_mem_manager(0);
  free_map(all_threads);
  delete_head(ready_q[curr_prio]);
  for (int i = 0; i < NUM_QUEUES; ++i) {
    free_list(ready_q[i]);
  }
  timer_delete(&sig_timer);
}

void swap_stack(my_pthread_t new_id) {
  if (new_id == 2) return;
  for (int i = 0; i < stack_page_size; ++i) {
    ht_val new_loc = ht_get(ht_space, new_id, i);
    if (new_loc == HT_NULL_VAL) {
      fprintf(stderr, "Could not find stack page %d of thread %d!\n", i,
              new_id);
      exit(1);
    }
    // unprotect relevant stack pages
    mprotect(mem_space + page_size*i, page_size, PROT_READ | PROT_WRITE);
    mprotect(mem_space + page_size*new_loc, page_size, PROT_READ | PROT_WRITE);
    swap(i,new_loc);
    // protect old stack page
    mprotect(mem_space + page_size*new_loc, page_size, PROT_NONE);
  }
}
