// File:  my_pthread_block.c
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"
#include "my_scheduler.h"

static uint32_t tid = 0;
static int initScheduler = 1; //if 1, initialize scheduler

timer_t* sig_timer;

void thread_func_wrapper(void* (*function)(void*), void* arg){
  tcb* currThread = (tcb*) get_head(ready_q);
  currThread->ret_val = function(arg);
  my_pthread_exit(NULL);
}

tcb* create_tcb(void* (*function)(void*), void* arg, ucontext_t* uc_link,
                my_pthread_t id) {
  ucontext_t* new_context = malloc(sizeof(ucontext_t));
  getcontext(new_context);
  new_context->uc_stack.ss_size = STACKSIZE;
  new_context->uc_stack.ss_sp = malloc(STACKSIZE);
  new_context->uc_link = uc_link;
  sigemptyset(&new_context->uc_sigmask);
  makecontext(new_context, (void (*)(void)) thread_func_wrapper, 2, function, arg);

  tcb* new_thread = (tcb*) malloc(sizeof(tcb));
  new_thread->id = id;
  new_thread->context = new_context;
  new_thread->ret_val = NULL;
  new_thread->waited_on = create_list();
  new_thread->waiting_on = NULL;
  new_thread->status= READY;
  return new_thread;
}

/* MEMORY LEAK:
 * Mallocs struct sigevent, struct sigaction, struct itimerspec without freeing.
 */
/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
  ucontext_t* curr_context = malloc(sizeof(ucontext_t));
  getcontext(curr_context);
  tcb* new_thread = create_tcb(function,arg,curr_context,++tid);
  
  if (initScheduler == 1) {
    ready_q = create_list();
    all_threads = create_map();

    // create tcb for current thread
    tcb* curr_thread = malloc(sizeof(tcb));
    curr_thread->id = tid++;
    curr_thread->ret_val = NULL;
    curr_thread->waited_on = create_list();
    curr_thread->waiting_on = NULL;
    curr_thread->context = curr_context;
    curr_thread->status = READY;
    put(all_threads, curr_thread->id, curr_thread);
    
    // add new thread to ready queue
    insert_head(ready_q, new_thread);
    // add current thread to ready queue head (must be head for execution to continue!)
    insert_head(ready_q, curr_thread);

    // create timer
    sig_timer = malloc(sizeof(timer_t));
    timer_create(CLOCK_THREAD_CPUTIME_ID, NULL, sig_timer);

    // register signal handler for alarms
    struct sigaction* act = malloc(sizeof(struct sigaction));
    act->sa_sigaction = schedule;
    act->sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&act->sa_mask);
    sigaction(SIGALRM, act, NULL);

    // set timer
    struct itimerspec* timer_25ms = malloc(sizeof(struct itimerspec));
    timer_25ms->it_interval.tv_nsec = QUANTUM;
    timer_25ms->it_interval.tv_sec = 0;
    timer_25ms->it_value.tv_nsec = QUANTUM;
    timer_25ms->it_value.tv_sec = 0;
    timer_settime(*sig_timer, 0, timer_25ms, NULL);
    initScheduler = 0;
  } else {
    insert_ready_q(new_thread);
  }
  // TODO: worry about concurrency when pushing to ready queue, manipulating TID
  *thread = new_thread->id;
  put(all_threads, new_thread->id, new_thread);
  return 0;
};


/* give CPU pocession to other user level thread_blocks voluntarily */
int my_pthread_yield() {
  return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) { 
  tcb* curr_thread = (tcb*) get_head(ready_q);
  if (value_ptr != NULL) value_ptr = curr_thread->ret_val;
  while (curr_thread->waited_on->head != NULL){
	  tcb* signal_thread = (tcb*) delete_head(curr_thread-> waited_on);
	  signal_thread->status = READY; 
	  insert_ready_q(signal_thread);
  }
  curr_thread->status = DONE;
  schedule(SIGALRM, NULL, curr_thread->context);
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
  if (tid < thread) {
	  return -1;
  }

  tcb* curr_thread = (tcb*) get_head(ready_q);
  tcb* t_block = get(all_threads, thread);
  if (t_block->status != DONE) {
	  insert_head(t_block -> waited_on, curr_thread);
	  curr_thread->status = BLOCKED;
  }
  schedule(SIGALRM, NULL, curr_thread->context);
  *value_ptr = t_block->ret_val;
  return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
  return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
  return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
  return 0;
};

