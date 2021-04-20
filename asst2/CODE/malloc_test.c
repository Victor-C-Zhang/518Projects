#include <stdio.h>
#include <stdlib.h>
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
//  long long n = 1000000000;
  long long n = 10000;
  int id = *((int*)ignored);
  while (n--) {
//    if (!(n%5000000)) printf("Thread %d: %lld\n", id, n);
  }
  printf("thread %d done\n", id);
  
/*  int len = 3;
  char* ptrs[len];
  for (int i = 0 ; i < len; i++) { 
    char* p = (char*) malloc(5);
    ptrs[i] = p;
   printf("id:%d, thread malloc %d\n", *((int*)ignored), i);
  }
//    printMemory();
  for (int i = 0 ; i < len; i++) { 
    free(ptrs[i]);
    printf("id:%d, thread free %d\n", *((int*)ignored), i);
  }
//    printMemory();
*/
}

void test_thread_create_join() {
  pthread_t other[3];
  void* ret_val[3]; 
  for (int i = 0; i < 3; i++) {
  	pthread_create(&other[i], NULL, thread_func, (void*)&i);
    	printf("thread create %d\n", i);
  }
  
  for (int i=0; i<3; i++) {
    pthread_join(other[i], &ret_val[i]);
    printf("thread join %d\n", i);
  }

}

void mallocTesting() {
  char* ptrs[50]; 
  for (int i = 0 ; i < 50; i++) { 
    char* p = (char*) myallocate(5, __FILE__, __LINE__, LIBRARYREQ);
    ptrs[i] = p;
    printMemory();
  }
  for (int i = 0 ; i < 50; i++) { 
    mydeallocate(ptrs[i], __FILE__, __LINE__, LIBRARYREQ);
    printMemory();
  }
}

int main(int argc, char** argv){
//  test_thread_create_join();
  mallocTesting();
  return 0;
}
