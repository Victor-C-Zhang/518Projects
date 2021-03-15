// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"
#include "my_scheduler.h"

int32_t tid = 0;

timer_t* sig_timer;

/* MEMORY LEAK:
 * Mallocs struct sigevent, struct sigaction, struct itimerspec without freeing.
 */
/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	ucontext_t* new_context = malloc(sizeof(ucontext_t));
  makecontext(new_context, *function, 1, arg);

  tcb* new_thread = malloc(sizeof(tcb));
  new_thread->id = tid++;
  new_thread->context = new_context;
  new_thread->retval = 69; // nice
  new_thread->waited_on = NULL;
  new_thread->waiting_on = NULL;

  if (ready == NULL) { // create new scheduler and peripherals
    ready = create_list();
    // create tcb for current thread
    tcb* curr_thread = malloc(sizeof(tcb));
    curr_thread->id = tid++;
    curr_thread->retval = 69; // nice
    curr_thread->waited_on = NULL;
    curr_thread->waiting_on = NULL;

    // add new thread to ready queue
    insert_head(ready, new_thread);
    // add current thread to ready queue head (must be head for execution to continue!)
    insert_head(ready, curr_thread);

    // create timer
    sig_timer = malloc(sizeof(timer_t));
    struct sigevent* sigev_config = malloc(sizeof(struct sigevent));
    sigev_config->sigev_notify = SIGEV_SIGNAL;
    sigev_config->sigev_signo = SIGALRM;
    timer_create(CLOCK_THREAD_CPUTIME_ID, sigev_config, sig_timer);

    // register signal handler for alarms
    struct sigaction* act = malloc(sizeof(struct sigaction));
    act->sa_sigaction = schedule;
    act->sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, act, NULL);

    // set timer
    struct itimerspec* timer_100ms = malloc(sizeof(struct itimerspec));
    timer_100ms->it_interval.tv_nsec = 100000000;
    timer_100ms->it_interval.tv_sec = 0;
    timer_100ms->it_value.tv_nsec = 100000000;
    timer_100ms->it_value.tv_sec = 0;
    timer_settime(sig_timer, 0, timer_100ms, NULL);
  } else {
    // TODO: insert to one after head
    insert_tail(ready, new_thread);
  }
  return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
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

