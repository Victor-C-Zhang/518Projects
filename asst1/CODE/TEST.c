#include "my_pthread_t.h"

void* thread_func(void* ignored) {
  long long n = 10000000;
  int id = *((int*)ignored);
  while (n--) {
    if (!(n%500000)) printf("Thread %d: %lld\n", id, n);
  }
  printf("thread %d done\n", id);
  return (void*)30;
}

int main() {
  pthread_t other[3];
  void* ret_val[3];
  pthread_create(&other[0], NULL, thread_func, (void*) &other[0]);
  printf("test: thread id %d created\n", other[0]);
  pthread_create(&other[1], NULL, thread_func, (void*) &other[1]);
  printf("test: thread id %d created\n", other[1]);
  pthread_create(&other[2], NULL, thread_func, (void*) &other[2]);
  printf("test: thread id %d created\n", other[2]);
  for (int i=0; i<3; i++) {
    printf("test: thread %d join\n", other[i]);
    pthread_join(other[i], &ret_val[i]);
    printf("test: thread %d returned %ld\n", other[i], (long int) ret_val[i]);
  }
  return 0;
}
