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
  char* t1 = myallocate(59*64 - 1, __FILE__, __LINE__, THREADREQ);
  printMemory();
  char* t2 = myallocate(5, __FILE__, __LINE__, THREADREQ);
  printMemory();
  char* t3 = myallocate(64*64 - 2, __FILE__, __LINE__, THREADREQ);
  printMemory();
  char* t4 = myallocate(64*64*25 + 64*12, __FILE__, __LINE__, THREADREQ);
  printMemory();
  for (int i = 0 ; i < 5; i++) {
    printf("%s\n", ptrs[i]);
    mydeallocate(ptrs[i], __FILE__, __LINE__, THREADREQ);
    printMemory();
  }
  mydeallocate(t3, __FILE__, __LINE__, THREADREQ);
  printMemory();
  char* t5 = myallocate(64*32 - 2, __FILE__, __LINE__, THREADREQ);
  printMemory();
  char* t6 = myallocate(3*64 + 12, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(t5, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(t1, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(t2, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(t4, __FILE__, __LINE__, THREADREQ);
  printMemory();
  mydeallocate(t6, __FILE__, __LINE__, THREADREQ);
  printMemory();
}

//contiguous allocation
void* TEST_thread_func_swap1(void* i) {
  printf("malloc %d\n", *((int*)i));
  char* p = (char*) myallocate(4095*sizeof(char), __FILE__, __LINE__, THREADREQ);
 // printMemory();
  
  long long n = 100000000;
  while (n--) {
  //  if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }

  printf("malloc %d\n", *((int*)i));
  char* f = (char*) myallocate(50*sizeof(char), __FILE__, __LINE__, THREADREQ);
 // printMemory();

  n = 100000000;
  while (n--) {
  //  if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }

  printf("free %d\n", *((int*)i));
  mydeallocate(p, __FILE__, __LINE__, THREADREQ);
 // printMemory();
  printf("free %d\n", *((int*)i));
  mydeallocate(f, __FILE__, __LINE__, THREADREQ);
 // printMemory();
}

//overflow allocation
void* TEST_thread_func_swap2(void* i) {
  printf("malloc %d\n", *((int*)i));
  char* p = (char*) myallocate(4031*sizeof(char), __FILE__, __LINE__, THREADREQ);
 // printMemory();

  printf("malloc %d\n", *((int*)i));
  char* f = (char*) myallocate(85*sizeof(char), __FILE__, __LINE__, THREADREQ);
  strcpy(f, "pizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazz");
 // printMemory();

  printf("malloc %d\n", *((int*)i));
  char* g = (char*) myallocate(85*sizeof(char), __FILE__, __LINE__, THREADREQ);
  strcpy(g, "pizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazzpizzazz");
  //printMemory();

  printf("free %d\n", *((int*)i));
  mydeallocate(f, __FILE__, __LINE__, THREADREQ);
 // printMemory();

  printf("free %d\n", *((int*)i));
  mydeallocate(p, __FILE__, __LINE__, THREADREQ);
//  printMemory();
  printf("free %d\n", *((int*)i));
  mydeallocate(g, __FILE__, __LINE__, THREADREQ);
//  printMemory();

}

//swapping overflow allocation
void* TEST_thread_func_swap3(void* i) {
  printf("malloc %d\n", *((int*)i));
  char* p = (char*) myallocate(4031*sizeof(char), __FILE__, __LINE__, THREADREQ);
//  printMemory();
  
  long long n = 100000000;
  while (n--) {
//    if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }


  printf("malloc %d\n", *((int*)i));
  char* f = (char*) myallocate(85*sizeof(char), __FILE__, __LINE__, THREADREQ);
  strcpy(f, "puzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzler");
  //printMemory();

  printf("malloc %d\n", *((int*)i));
  char* g = (char*) myallocate(85*sizeof(char), __FILE__, __LINE__, THREADREQ);
  strcpy(g, "puzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzlerpuzzler");
//  printMemory();
  
  n = 100000000;
  while (n--) {
  //  if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }

  printf("free %d\n", *((int*)i));
  mydeallocate(p, __FILE__, __LINE__, THREADREQ);
//  printMemory();
  printf("free %d\n", *((int*)i));
  mydeallocate(f, __FILE__, __LINE__, THREADREQ);
//  printMemory(); 

  printf("free %d\n", *((int*)i));
  mydeallocate(g, __FILE__, __LINE__, THREADREQ);
  //printMemory(); 
}


void TEST_thread_swap( void*(f(void*))) {

  int len = 2;
  pthread_t other[len];
  void* ret_val[len]; 
  for (int i = 0; i < len; i++) {
  	pthread_create(&other[i], NULL, f, (void*)&i);
    	printf("thread create %d\n", i);
  }
  
  for (int i=0; i < len; i++) {
    pthread_join(other[i], &ret_val[i]);
  }
}

