// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
// #include <ucontext.h>
// #include "my_pthread_t.h"

// void printInt(void* data){
// 	printf("%d ", *(int *)data); 
// }

// void freeList(linked_list_t* list) {
// 	while (list->head != NULL) {
// 		delete_head(list);
// 	}
// 	free(list);
// }

// void testLinkedList() {
// 	linked_list_t* list = create_list();
// 	int i = 2;
// 	int i2 = 3;
// 	int i3 = 1;
// 	int i4 = 4;
// 	insert_head(list, (void*) &i);
// 	insert_tail(list, (void*) &i2);
// 	insert_head(list, (void*) &i3);
// 	insert_tail(list, (void*) &i4);
// 	print_list(list, printInt);
// 	printf("head: %d \n", *(int *)get_head(list)); 
// 	printf("tail: %d \n", *(int *)get_tail(list)); 
// 	freeList(list);
// 	return;
// }

// void testHashMap() {
// 	hashmap* map = create_map();
// 	tcb* t1 = (tcb*) malloc(sizeof(tcb));
// 	tcb* t2 = (tcb*) malloc(sizeof(tcb));
// 	tcb* t3 = (tcb*) malloc(sizeof(tcb));
// 	tcb* t1p = put(map, 123, t1);
// 	tcb* t2p = put(map, 333, t2);
// 	tcb* t3p = put(map, 123, t3);
// 	printf("%d\n", t1p == t1);
// 	printf("%d\n", t2p == t2);
// 	printf("%d\n", t3p == t1);	
// 	printf("%d\n", t2 == get(map, 333));
// 	printf("%d\n", t3 == get(map, 123));
// 	free(t1);
// 	free(t2);
// 	free(t3);
// 	free_map(map);
// 	return;
// }

void sig_handler(int sig, siginfo_t* info, void* ucontext) {
	if (sig != SIGALRM) {
		printf("Oops! Not SIGALRM\n");
		exit(69);
	}
	printf("Timer %ld\n", clock());
}

void test_alarm() {
	// timer_t* sig_timer = malloc(sizeof(timer_t));
  struct sigevent* sigev_config = malloc(sizeof(struct sigevent));
  sigev_config->sigev_notify = SIGEV_SIGNAL;
  sigev_config->sigev_signo = SIGALRM;
  timer_create(CLOCK_THREAD_CPUTIME_ID, sigev_config, sig_timer);

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
  timer_settime(sig_timer, 0, timer_100ms, NULL);
}

int main(int argc, char** argv){
	// testLinkedList();
	// testHashMap();
	test_alarm();
	return 0;
}
