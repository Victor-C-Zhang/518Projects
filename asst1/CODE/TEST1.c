#include "my_pthread_t.h"

typedef struct params {
  pthread_mutex_t* lock;
  uint32_t* id; 
} params;


void* thread_func_mutex(void* args) {
  params* vals = (params*)args;
  pthread_mutex_t* lock  = (pthread_mutex_t*)vals->lock;
  uint32_t id = *( (uint32_t*)vals->id );
  int i = 3;
  while ( i-- ) {
    printf("Thread %d about to lock\n", id);
    long long n = 10000000000;
    pthread_mutex_lock(lock);
    while (n--) {
      if (!(n%50000000)) printf("Thread %d: %lld\n", id, n);
    }
    pthread_mutex_unlock(lock);
    printf("Thread %d unlocked\n", id);
  }
  return (void*)30;
}


void test_mutex(){
  pthread_mutex_t lock;
  pthread_mutex_init(&lock, NULL);
  int len = 3;
  my_pthread_t threads[len];
  params* args[len];
  for (int i = 0; i < len; i++) {
    args[i] = (params*) malloc(sizeof(params));
    args[i] -> lock = &lock;
    args[i] -> id = &threads[i];
    pthread_create(&threads[i], NULL, thread_func_mutex, (void*)args[i]);
    printf("id: %d created\n", threads[i]);
  }
  
  for (int i = 0; i <len; i++) {
    printf("id: %d join\n", threads[i]);
    pthread_join(threads[i], NULL);
  }
  
  for (int i = 0; i <len; i++) {
    free(args[i]);
  }

  pthread_mutex_destroy(&lock);
}


int main() {
  test_mutex();
  return 0;
}
