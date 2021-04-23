#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_pthread_t.h"
#include "my_scheduler.h"
#include "my_malloc.h"

typedef struct params {
//  pthread_mutex_t* lock;
  uint32_t* id; 
} params;

void printInt(void* data){
  printf("%d ", *(int *)data); 
}


void* thread_func(void* ignored) {
/*  long long n = 1000000000;
  int id = *((int*)ignored);
  while (n--) {
    if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }
 
//  printf("thread %d done\n", id);
 */ 
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

void test_thread_create_join() {
  int len = 3;
  pthread_t other[len];
  void* ret_val[len]; 
  for (int i = 0; i < len; i++) {
  	pthread_create(&other[i], NULL, thread_func, (void*)&i);
    	printf("thread create %d\n", i);
  }
  
  for (int i=0; i < len; i++) {
    pthread_join(other[i], &ret_val[i]);
  }

}

void mallocTesting() {
  char* ptrs[5]; 
  for (int i = 0 ; i < 5; i++) { 
    char* p = (char*) myallocate(5*sizeof(char), __FILE__, __LINE__, LIBRARYREQ);
    strcpy(p,"help"); 
    ptrs[i] = p;
    printMemory();
  }
  for (int i = 0 ; i < 5; i++) { 
//    printf("%s\n", ptrs[i]);
    mydeallocate(ptrs[i], __FILE__, __LINE__, LIBRARYREQ);
    printMemory();
  }
}

int main(int argc, char** argv){
  test_thread_create_join();
//  mallocTesting();
  printf("done\n");
  return 0;

}
