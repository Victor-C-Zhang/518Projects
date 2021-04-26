#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_pthread_t.h"
#include "my_scheduler.h"
#include "my_malloc.h"
#include "test_runner.h"

typedef struct params {
//  pthread_mutex_t* lock;
  uint32_t* id; 
} params;

void* malloc_thread_func(void* ignored) {
  int len = 3;
  char* ptrs[len];
  for (int i = 0 ; i < len; i++) { 
//    char* p = (char*) malloc(5);
    char* p = (char*) myallocate(5, __FILE__, __LINE__, THREADREQ);
    ptrs[i] = p;
   printf("id:%d, thread malloc %d\n", *((int*)ignored), i);
  }
    printMemory();
  for (int i = 0 ; i < len; i++) { 
    mydeallocate(ptrs[i], __FILE__, __LINE__, THREADREQ);
    //free(ptrs[i]);
    printf("id:%d, thread free %d\n", *((int*)ignored), i);
  }
  printMemory();
}

void TEST_malloc_thread_create_join() {
  int len = 3;
  pthread_t other[len];
  void* ret_val[len]; 
  for (int i = 0; i < len; i++) {
  	pthread_create(&other[i], NULL, malloc_thread_func, (void*)&i);
    	printf("thread create %d\n", i);
  }
  
  for (int i=0; i < len; i++) {
    pthread_join(other[i], &ret_val[i]);
  }
}

void TEST_malloc_directmapping() {
  char* ptrs[5]; 
  for (int i = 0 ; i < 5; i++) { 
    char* p = (char*) myallocate(5*sizeof(char), __FILE__, __LINE__, THREADREQ);
    strcpy(p,"help"); 
    ptrs[i] = p;
    printMemory();
  }
  for (int i = 0 ; i < 5; i++) { 
//    printf("%s\n", ptrs[i]);
    mydeallocate(ptrs[i], __FILE__, __LINE__, THREADREQ);
    printMemory();
  }
}

void* TEST_thread_func_swap1(void* int) {
  char* p = (char*) myallocate(4090*sizeof(char), __FILE__, __LINE__, THREADREQ);
  printMemory();
  
  long long n = 100000000;
  while (n--) {
  //  if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }

  char* f = (char*) myallocate(40*sizeof(char), __FILE__, __LINE__, THREADREQ);
  printMemory();

  long long n = 100000000;
  while (n--) {
  //  if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }

  mydeallocate(p, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(f, __FILE__, __LINE__, THREADREQ);
  printMemory();
}

void TEST_thread_swap1() {
  int len = 2;
  pthread_t other[len];
  void* ret_val[len]; 
  for (int i = 0; i < len; i++) {
  	pthread_create(&other[i], NULL, TEST_thread_func_swap1, NULL);
    	printf("thread create %d\n", i);
  }
  
  for (int i=0; i < len; i++) {
    pthread_join(other[i], &ret_val[i]);
  }
}
