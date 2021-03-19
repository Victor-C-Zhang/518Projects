// File:  my_pthread.c
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include "my_pthread_t.h"
#include "my_scheduler.h"
typedef struct params {
	my_pthread_mutex_t* lock;
	uint32_t* id; 
} params;
void printInt(void* data){
  printf("%d ", *(int *)data); 
}

void freeList(linked_list_t* list) {
  while (list->head != NULL) {
    delete_head(list);
  }
  free(list);
}

void testLinkedList() {
  linked_list_t* list = create_list();
  int i = 2;
  int i2 = 3;
  int i3 = 1;
  int i4 = 4;
  insert_head(list, (void*) &i);
  insert_tail(list, (void*) &i2);
  insert_head(list, (void*) &i3);
  insert_tail(list, (void*) &i4);
  print_list(list, printInt);
  printf("head: %d \n", *(int *)get_head(list)); 
  printf("tail: %d \n", *(int *)get_tail(list)); 
  freeList(list);
  return;
}

void testHashMap() {
  hashmap* map = create_map();
  tcb* t1 = (tcb*) malloc(sizeof(tcb));
  tcb* t2 = (tcb*) malloc(sizeof(tcb));
  tcb* t3 = (tcb*) malloc(sizeof(tcb));
  tcb* t1p = put(map, 123, t1);
  tcb* t2p = put(map, 333, t2);
  tcb* t3p = put(map, 123, t3);
  printf("%d\n", t1p == t1);
  printf("%d\n", t2p == t2);
  printf("%d\n", t3p == t1);
  printf("%d\n", t2 == get(map, 333));
  printf("%d\n", t3 == get(map, 123));
  free(t1);
  free(t2);
  free(t3);
  free_map(map);
  return;
}

void sig_handler(int sig, siginfo_t* info, void* ucontext) {
  if (sig != SIGALRM) {
    printf("Oops! Not SIGALRM\n");
    exit(69);
  }
  printf("Timer %ld\n", clock());
}

void test_alarm() {
  timer_t* sig_timer = malloc(sizeof(timer_t));
  timer_create(CLOCK_THREAD_CPUTIME_ID, NULL, sig_timer);

  // register signal handler for alarms
  struct sigaction* act = malloc(sizeof(struct sigaction));
  act->sa_sigaction = sig_handler;
  act->sa_flags = SA_SIGINFO;
  sigaction(SIGALRM, act, NULL);

  // set timer
  struct itimerspec* timer_100ms = malloc(sizeof(struct itimerspec));
  timer_100ms->it_interval.tv_nsec = 100000000;
  timer_100ms->it_interval.tv_sec = 0;
  timer_100ms->it_value.tv_nsec = 100000000;
  timer_100ms->it_value.tv_sec = 0;
  timer_settime(*sig_timer, 0, timer_100ms, NULL);

  printf("Timer set, looping\n");
  long long int bignum = 500000000;
  long long int mod = 10e9+7;
  int a = 2;

  while (bignum--) {
    a = a*3;
    a%=mod;
    if (!(a%69420)) printf("Nice\n");
  }
}

void* thread_func(void* ignored) {
  long long n = 1000000000;
  int id = *((int*)ignored);
 // ucontext_t* new_context = malloc(sizeof(ucontext_t));
 // getcontext(new_context);
  while (n--) {
    if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }
  printf("thread %d done\n", id);
  return (void*)30;
}

void test_thread_create() {
  my_pthread_t other;
  my_pthread_create(&other, NULL, thread_func, (void*)&other);
  long long n = 1000000000;
  while (n--) {
    if (!(n%5000000)) printf("Main: %lld\n",n);
  }
}

void test_thread_create_join() {
  my_pthread_t other[3];
  void* ret_val[3]; 
  my_pthread_create(&other[0], NULL, thread_func, (void*) &other[0]);
  printf("test: thread id %d created\n", other[0]);
  my_pthread_create(&other[1], NULL, thread_func, (void*) &other[1]);
  printf("test: thread id %d created\n", other[1]);
  my_pthread_create(&other[2], NULL, thread_func, (void*) &other[2]);
  printf("test: thread id %d created\n", other[2]);
  for (int i=0; i<3; i++) {
	  printf("test: thread %d join\n", other[i]);
  	my_pthread_join(other[i], &ret_val[i]);
  	printf("test: thread %d returned %ld\n", other[i], (long int) ret_val[i]); 
  }
}

void* thread_func_mutex(void* args) {
  long long n = 1000000000;
  params* vals = (params*)args;
  my_pthread_mutex_t* lock  = (my_pthread_mutex_t*)vals->lock;
  uint32_t id = *( (uint32_t*)vals->id );
  printf("Thread %d about to lock\n", id);
  my_pthread_mutex_lock(lock);
  while (n--) {
    if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }
  my_pthread_mutex_unlock(lock);
  return (void*)30;
}

void testMutex(){
	my_pthread_mutex_t lock;
	my_pthread_mutex_init(&lock, NULL);
	my_pthread_t threads[5];	
	params* args[5];
	for (int i = 0; i <5; i++) {
		args[i] = (params*) malloc(sizeof(params));
		args[i] -> lock = &lock;
		args[i] -> id = &threads[i];
		my_pthread_create(&threads[i], NULL, thread_func_mutex, (void*)args[i]);
		printf("id: %d created\n", threads[i]);
	}
	
	for (int i = 0; i <5; i++) {
		printf("id: %d join\n", threads[i]);
		my_pthread_join(threads[i], NULL);
	}
	
	for (int i = 0; i <5; i++) {
		free(args[i]);
	}

	my_pthread_mutex_destroy(&lock);
}

void* yield_thread_func(void* ignored) {
  long long n = 1000000000;
  printf("Before yield\n");
  while (1) my_pthread_yield();
  // should be inaccessible
  printf("After yield\n");
  while (n--) {
    if (!(n%5000000)) printf("Thread: %lld\n",n);
  }
  return (void*)30;
}

void test_thread_yield() {
  my_pthread_t other1;
  my_pthread_t other2;
  my_pthread_create(&other1, NULL, yield_thread_func, NULL);
  my_pthread_create(&other2, NULL, yield_thread_func, NULL);
  long long n = 1000000000;
  while (n--) {
    if (!(n%5000000)) printf("Main: %lld\n",n);
  }
}


int main(int argc, char** argv){
/*  testLinkedList();
  testHashMap();
  test_alarm();
*/  	
//  test_thread_create();
  testMutex();
//  test_thread_create_join();
//  test_thread_yield();
  return 0;
}
