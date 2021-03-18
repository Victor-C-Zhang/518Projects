// File:  my_pthread_block.c
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"

static uint32_t tid = 0;
static int initScheduler = 1; //if 1, initialize scheduler

typedef struct mut_wait_list {
	tcb* thread;
	int priority;
} mut_wait_list;

void thread_func_wrapper(void* (*function)(void*), void* arg){
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  curr_thread->ret_val = function(arg);
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
  new_thread->last_run = cycles_run;
  return new_thread;
}

/* MEMORY LEAK:
 * Mallocs struct sigevent, struct sigaction without freeing.
 */
/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
  ucontext_t* curr_context = malloc(sizeof(ucontext_t));
  getcontext(curr_context);
  tcb* new_thread = create_tcb(function,arg,curr_context,++tid);
  	//no thread has 0 tid
  if (initScheduler == 1) {
    for (int i = 0; i < NUM_QUEUES; ++i) {
      ready_q[i] = create_list();
    }
    all_threads = create_map();
    should_maintain = ONE_SECOND/QUANTUM;
    // create tcb for current thread
    tcb* curr_thread = malloc(sizeof(tcb));
    curr_thread->id = ++tid; //no thread has 0 tid
    curr_thread->ret_val = NULL;
    curr_thread->waited_on = create_list();
    curr_thread->waiting_on = NULL;
    curr_thread->context = curr_context;
    curr_thread->status = READY;
    put(all_threads, curr_thread->id, curr_thread);
    put(all_threads, new_thread->id, new_thread);

    *thread = new_thread->id;
//    printf("sched thread %d\n", curr_thread->id);
    // add new thread to ready queue
    insert_head(ready_q[0], new_thread);
    // add current thread to ready queue head (must be head for execution to continue!)
    insert_head(ready_q[0], curr_thread);

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
    timer_settime(*sig_timer, 0, &timer_25ms, NULL);
    initScheduler = 0;
  } else {
    enter_scheduler(&timer_pause_dump);
    insert_ready_q(new_thread,0);
    *thread = new_thread->id;
    put(all_threads, new_thread->id, new_thread);
    exit_scheduler(&timer_pause_dump);
  }
//  printf("new thread %d\n", new_thread->id);
  return 0;
};


/* give CPU pocession to other user level thread_blocks voluntarily */
int my_pthread_yield() {
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  curr_thread->cycles_left = -1; // tell the scheduler the scheduling is caused
  // by yield
  raise(SIGALRM);
  return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  if (value_ptr != NULL) value_ptr = curr_thread->ret_val;
  while (curr_thread->waited_on->head != NULL){
	  tcb* signal_thread = (tcb*) delete_head(curr_thread-> waited_on);
	  signal_thread->status = READY; 
	  insert_head(ready_q[signal_thread->cycles_left], signal_thread);
  }
  curr_thread->status = DONE;
  raise(SIGALRM);
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
  if (tid < thread) {
	  return -1;
  }
  enter_scheduler(&timer_pause_dump);
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  tcb* t_block = get(all_threads, thread);
//  printf("curr thread: %d join w %d\n", curr_thread->id, thread);
  if (t_block == NULL) {
  //	printf("block NULL\n");
	return -1;
  }
  if (t_block->status != DONE) {
	  insert_head(t_block -> waited_on, curr_thread);
	  curr_thread->status = BLOCKED;
  }

  raise(SIGALRM);
  if (value_ptr != NULL){
    *value_ptr = t_block->ret_val;
  }
  return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  mutex->locked = 0;
  mutex->owner = 0;
  mutex->hoisted_priority = INT8_MAX;
  mutex->leftover_cycles = 0;
  mutex->waiting_on = create_list();
  return 0;
};

void mutex_lock(my_pthread_mutex_t* mutex, tcb* thread, int cycles){
  mutex->locked = 1;
  mutex->owner = thread->id; 
  mutex->leftover_cycles = cycles;
  thread -> waiting_on = NULL;
  thread -> status = READY;
//  printf("thread %d got lock \n", mutex->owner);

}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) { 
  if (mutex->waiting_on == NULL) { //mutex been destroyed
  	return -1;
  }
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);

  if (mutex->locked == 1){
      enter_scheduler(&timer_pause_dump);
  //    printf("locked\n");
      if ( mutex->hoisted_priority > curr_prio) mutex->hoisted_priority = curr_prio;
      curr_thread->waiting_on = mutex;
      mut_wait_list* pairT = malloc(sizeof(pairT));
      pairT -> thread = curr_thread;
      pairT -> priority = curr_prio;
      insert_tail(mutex->waiting_on, pairT);
      curr_thread->status = BLOCKED;
      raise(SIGALRM);
  }
  else {
	//when here, hoisted_prio will be INT8_MAX, aka not set, so don't need to subtract
	mutex_lock(mutex,  curr_thread, curr_prio);
//	printf("thread %d got lock \n", mutex->owner);
	//update hoisted priority ??? 
  }

  return 0;
};

/* release the mutex lock */
/*what if hoisted priority thread isn't waiting anymore, 
 * should hoisted priority update to next highest value? 
 */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) { 
  if (mutex->waiting_on == NULL) { //mutex been destroyed
  	return -1;
  }
  tcb* curr_thread = (tcb*) get_head(ready_q[curr_prio]);
  if ( (mutex -> locked) && (mutex->owner == curr_thread->id)) {

  	curr_thread->cycles_left = curr_thread->cycles_left + mutex->leftover_cycles;

	if ( get_head(mutex->waiting_on) != NULL) {
		mut_wait_list* signal_pair = (mut_wait_list*) delete_head( mutex -> waiting_on);
		mutex_lock(mutex, signal_pair->thread, (signal_pair->priority-mutex->hoisted_priority) );
		insert_head(ready_q[mutex->hoisted_priority], signal_pair->thread); 
		free(signal_pair);
	}
	else {
  		mutex->locked = 0;
		mutex->owner = 0;
  		mutex->hoisted_priority = INT8_MAX;
	}
  }
 return 0;
};

/* destroy the mutex */
//seg faults ???
//edge case: calling thread does not join and destroys before the called threads are done using it
//need to wait for all threads w/ access to mutex to be done?
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	if (mutex == NULL) return -1;
	if (mutex->locked == 1 || mutex->waiting_on == NULL) return -1;
	while ( get_head(mutex->waiting_on) != NULL) {
		free(delete_head(mutex->waiting_on));
	}
	free(mutex->waiting_on);
	return 0;
};

