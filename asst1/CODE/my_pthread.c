// File:	my_pthread_block.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "my_pthread_block_t.h"

static int initScheduler = 1; //if 1, initialize scheduler 
static uint32_t currId = 0;

void thread_func_wrapper(void* (*function)(void*), void* arg){
	tcb* currThread = get_active_thread();
	currThread -> ret_val = function(arg);
}

/* create a new thread_block */
int my_pthread_block_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	if (initScheduler == 1) {
		//set sigmask in uc_sigmask
		initScheduler = 0;
	}
	
	tcb* thread_block = (tcb*) malloc(sizeof(tcb));
	thread_block->id = ++currId;
	*thread = thread_block->id;
	thread_block->stack_ptr = malloc(STACKSIZE);
	thread_block->context = (ucontext_t*)malloc(sizeof(ucontext_t));
	thread_block->context->uc_stack.ss_sp = thread->stack_ptr;
	thread_block->context->uc_stack.ss_size = STACKSIZE;
	thread_block->context->uc_link = scheduler_tcb->context;
	getcontext(thread_block->context);
	makecontext(thread_block->context, thread_func_wrapper, 2, function, arg);
	insert_tail(ready_q, tcb);
	tcb* currThread = (tcb*) get_active_thread();
	return swapcontext(get_head(ready_q)->context, scheduler_tcb->context);
};


/* give CPU pocession to other user level thread_blocks voluntarily */
int my_pthread_block_yield() {
	return 0;
};

/* terminate a thread_block */
void my_pthread_block_exit(void *value_ptr) {
};

/* wait for thread_block termination */
int my_pthread_block_join(my_pthread_t thread, void **value_ptr) {
	return 0;
};

/* initial the mutex lock */
int my_pthread_block_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	return 0;
};

/* aquire the mutex lock */
int my_pthread_block_mutex_lock(my_pthread_mutex_t *mutex) {
	return 0;
};

/* release the mutex lock */
int my_pthread_block_mutex_unlock(my_pthread_mutex_t *mutex) {
	return 0;
};

/* destroy the mutex */
int my_pthread_block_mutex_destroy(my_pthread_mutex_t *mutex) {
	return 0;
};

