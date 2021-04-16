#include <stdio.h>
#include <stdlib.h>
#include "my_pthread_t.h"
#include "my_scheduler.h"
#include "my_malloc.h"

typedef struct params {
  pthread_mutex_t* lock;
  uint32_t* id; 
} params;

void printInt(void* data){
  printf("%d ", *(int *)data); 
}


void* thread_func(void* ignored) {
  char* ptrs[50]; 
  for (int i = 0 ; i < 50; i++) { 
    ptrs[i] = (char*) malloc(5);
    ptrs[i] = "help";
   printf("thread malloc %d\n", i);
  }
  for (int i = 0 ; i < 50; i++) { 
    free(ptrs[i]);
    printf("thread free %d\n", i);
  }
}

void test_thread_create_join() {
  pthread_t other[3];
  void* ret_val[3]; 
  for (int i = 0; i < 3; i++) {
  	pthread_create(&other[i], NULL, thread_func, (void*) &other[i]);
  }
  
  for (int i=0; i<3; i++) {
    pthread_join(other[i], &ret_val[i]);
  }

}

int main(int argc, char** argv){
  test_thread_create_join();
  return 0;
}
