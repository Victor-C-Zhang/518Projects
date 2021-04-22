// File:  my_pthread.c
// Author:  Yujie REN
// Date:  09/23/2017

// name: Bharti Mehta, Victor Zhang
// username of iLab: bam270
// iLab Server: pwd

#include "my_pthread_t.h"
#include "my_scheduler.h"

static uint32_t tid = 0;
static int initScheduler = 1; //if 1, initialize scheduler

// wraps user-provided func to ensure pthread_exit is called
void thread_func_wrapper(void* (*function)(void*), void* arg) {
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  curr_thread->ret_val = function(arg);
  my_pthread_exit(curr_thread->ret_val);
}

tcb* create_tcb(void* (*function)(void*), void* arg, my_pthread_t id) {
  tcb* new_thread = (tcb*) malloc(sizeof(tcb));
//  tcb* new_thread = (tcb*) myallocate(sizeof(tcb), __FILE__, __LINE__, LIBRARYREQ);
  new_thread->id = id;
  new_thread->ret_val = NULL;
  new_thread->status= READY;
  new_thread->priority = 0;
  new_thread->cycles_left = 0;
  new_thread->acq_locks = 0;
  new_thread->last_run = cycles_run;
  new_thread->waited_on = create_list();

  getcontext(&new_thread->context);
  new_thread->context.uc_stack.ss_size = STACKSIZE;
//  new_thread->context.uc_stack.ss_sp = myallocate(STACKSIZE, __FILE__, __LINE__, LIBRARYREQ);
  new_thread->context.uc_stack.ss_sp = malloc(STACKSIZE);
  sigemptyset(&new_thread->context.uc_sigmask);
  makecontext(&new_thread->context, (void (*)(void)) thread_func_wrapper, 2, function, arg);

  return new_thread;
}

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
  tcb* new_thread = create_tcb(function,arg,++tid); //no thread has 0 tid
  if (initScheduler) { // spin up a new scheduler
    atexit(free_data);
    for (int i = 0; i < NUM_QUEUES; ++i) {
      ready_q[i] = create_list();
    }
    all_threads = create_map();
    should_maintain = ONE_SECOND/QUANTUM;
    prev_done = NULL;
    // create tcb for current thread
    tcb* curr_thread = malloc(sizeof(tcb));
 //   tcb* curr_thread = myallocate(sizeof(tcb), __FILE__, __LINE__, LIBRARYREQ);

    curr_thread->id = ++tid; //no thread has 0 tid
    getcontext(&curr_thread->context);
    curr_thread->ret_val = NULL;
    curr_thread->status = READY;
    curr_thread->priority = 0;
    curr_thread->cycles_left = 0;
    curr_thread->acq_locks = 0;
    curr_thread->waited_on = create_list();
    put(all_threads, curr_thread->id, curr_thread);
    put(all_threads, new_thread->id, new_thread);

    *thread = new_thread->id;
    // add new thread to ready queue
    insert_head(ready_q[0], new_thread);
    // add current thread to ready queue head (must be head for execution to continue!)
    insert_head(ready_q[0], curr_thread);

    // create timer
    timer_create(CLOCK_THREAD_CPUTIME_ID, NULL, &sig_timer);

    // register signal handler for alarms

//    act = myallocate(sizeof(struct sigaction), __FILE__, __LINE__, LIBRARYREQ);
    act.sa_sigaction = schedule;
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);

    // set timer
    timer_settime(sig_timer, 0, &timer_25ms, NULL);
    initScheduler = 0;
  } else { // no need to init scheduler, can just add thread normally
    enter_scheduler(&timer_pause_dump);
    insert_ready_q(new_thread,0);
    *thread = new_thread->id;
    put(all_threads, new_thread->id, new_thread);
    exit_scheduler(&timer_pause_dump);
  }
  return 0;
}


/* give CPU pocession to other user level thread_blocks voluntarily */
int my_pthread_yield() {
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  curr_thread->cycles_left = -1; // tell the scheduler the scheduling is caused
  // by yield
  raise(SIGALRM);
  return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
  //printf("thread exit start\n");
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  curr_thread->ret_val = value_ptr;
  // notify waiting threads
  while (curr_thread->waited_on->head != NULL){
    tcb* signal_thread = (tcb*) delete_head(curr_thread-> waited_on);
    signal_thread->status = READY; 
    insert_ready_q(signal_thread, signal_thread->priority);
  }
  free_list(curr_thread->waited_on);
  curr_thread->status = DONE;
//  printf("thread exit done\n");
  raise(SIGALRM);
}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
//  printf("thread join\n");
  if (tid < thread) {
    return -1;
  }
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  tcb* t_block = get(all_threads, thread);
  if (t_block == NULL) {
    return -1; // cannot wait on nonexistent thread
  }
  if (t_block->status != DONE) { // block and wait
    insert_head(t_block -> waited_on, curr_thread);
    curr_thread->status = BLOCKED;
  //  printf("thread join schedule\n");
    raise(SIGALRM);
  }
  // by this point, t_block will be done
  if (value_ptr != NULL){
    *value_ptr = t_block->ret_val;
  }
  
//  printf("thread join done\n");
  return 0;
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  mutex->locked = 0;
  mutex->owner = 0;
  mutex->hoisted_priority = INT8_MAX;
  mutex->holding_thread_priority = 0;
  return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
  enter_scheduler(&timer_pause_dump);
  while (__atomic_exchange_n(&mutex->locked, 1, __ATOMIC_ACQ_REL)) {
    // hoist priority of mutex
    tcb* curr_thread = (tcb*)get_head(ready_q[curr_prio]);
    if ( mutex->hoisted_priority > curr_thread->priority) { // change the
      // priority of the thread holding the mutex
      mutex->hoisted_priority = curr_thread->priority;
      tcb* holding_thread = (tcb*) get(all_threads, mutex->owner);
      if (holding_thread->status == DONE) continue; // auto-assign locks from
      // dead threads
      if (holding_thread->priority > curr_thread->priority) {
        int prio = holding_thread->priority;
        holding_thread->priority = curr_thread->priority;
        if (holding_thread->status == READY) { // remove thread from queue
          // and add to faster queue
          node_t* ptr = ready_q[prio]->head;
          node_t* prev_ptr = NULL;
          while (ptr != NULL) {
            tcb *thread = (tcb *) ptr->data;
            if (thread == holding_thread) {
              insert_ready_q(thread, curr_thread->priority);
              if (ptr == ready_q[prio]->head) {
                ready_q[prio]->head = ptr->next;
              } else {
                prev_ptr->next = ptr->next;
              }
              if (ptr == ready_q[prio]->tail) {
                ready_q[prio]->tail = prev_ptr;
              }
              free(ptr);
              break;
            }
            prev_ptr = ptr;
            ptr = ptr->next;
          }
        }
      }
    }
    my_pthread_yield();
  }
  // got the mutex
  tcb* curr_thread = (tcb*)get_head(ready_q[curr_prio]);
  mutex->owner = curr_thread->id;
  if (mutex->hoisted_priority > curr_thread->priority) {
    mutex->hoisted_priority = curr_thread->priority;
  }
  if (mutex->hoisted_priority == curr_prio) { // no need to keep hoisting
    mutex->hoisted_priority = INT8_MAX;
    mutex->holding_thread_priority = curr_prio;
  } else if (mutex->hoisted_priority < curr_thread->priority) { // hoist priority
    curr_thread->cycles_left += mutex->hoisted_priority - curr_thread->priority;
    mutex->holding_thread_priority = curr_thread->priority;
    curr_thread->priority = mutex->hoisted_priority;
    // temporary mismatch between curr_prio and thread prio is ok. will be
    // fixed before any other thread runs
  }
  ++curr_thread->acq_locks;
  exit_scheduler(&timer_pause_dump);
  return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
  enter_scheduler(&timer_pause_dump);
  mutex->locked = 0;
  tcb* curr_thread = (tcb*)get_head(ready_q[curr_prio]);
  --curr_thread->acq_locks;
  // revoke hoisted prio if necessary
  if (mutex->hoisted_priority < mutex->holding_thread_priority) {
    curr_thread->cycles_left -= mutex->holding_thread_priority -
          mutex->hoisted_priority;
    curr_thread->priority = mutex->holding_thread_priority;
    if (curr_thread->cycles_left < 0) curr_thread->cycles_left = 0;
  }
  exit_scheduler(&timer_pause_dump);
  return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
  if (mutex == NULL) return -1;
  if (__atomic_exchange_n(&mutex->locked, 1, __ATOMIC_ACQ_REL)) return -1;
  return 0;
}
