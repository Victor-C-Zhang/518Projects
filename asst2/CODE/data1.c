#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "my_pthread_t.h"

typedef struct my_params {
  float multiple;
  int isLock; //1 is yes
  pthread_mutex_t* lock;
} my_params;

FILE *fp;
clock_t start;
clock_t stop;
int totalTime = 10;
int multMin = 1;
int multTot = 1;
int lockMin = 0;
int lockTot = 0;
int threadMin = 13;
int threadTot = 15;
char* fName = "my_library_data.csv";

void startTime(){ 
  start = clock();  
}

void endTime(){ 
  stop = clock();
}

float durTime(){
  float dur = ( (float) (stop - start) )/CLOCKS_PER_SEC;
  return dur;
}

void* my_thread_func(void* args) {

  my_params* vals = (my_params*)args;
  long long n = (long long) 1000000000 * vals->multiple;
  int isLock = vals->isLock;
  pthread_mutex_t* lock  = (pthread_mutex_t*)vals->lock;
  if (isLock)  pthread_mutex_lock(lock);
  while (n--) {} // spin
  if (isLock)  pthread_mutex_unlock(lock);
  return (void*)30;
}


float pthread_data(int mult, int isLock, int threadNum) {
  startTime();
  pthread_t threads[threadNum];
  my_params* args[threadNum];
  pthread_mutex_t lock;
  pthread_mutex_init(&lock, NULL);
  for (int i = 0; i < threadNum; i++){
    args[i] = (my_params*) malloc(sizeof(my_params));
    args[i] -> lock = &lock;
    args[i] -> multiple = mult * .5f;
    args[i] -> isLock = isLock;
    pthread_create(&threads[i], NULL, my_thread_func, (void*)args[i]);
  }
  for (int i = 0; i < threadNum; i++) {
    pthread_join(threads[i], NULL);
  } 
  for (int i = 0; i < threadNum; i++) {
    free(args[i]);
  }
  pthread_mutex_destroy(&lock);
  endTime();
  return durTime();
}

int main(int argc, char** argv){
  fp=fopen(fName,"w+"); 
  fprintf(fp,"Multiple, Synchronized, Thread Num, pthread Avg Time, Reg pthread Avg Time\n");
  for (int mult = multTot; mult >= multMin; mult--){  
    for (int isLock = lockMin; isLock <= lockTot; isLock++) {
      for (int threadNum = threadMin; threadNum <= threadTot; threadNum++) {
        float my_time = 0; 
        float reg_time = 0; 
        for (int numTimes = 1; numTimes <= totalTime; numTimes++) {
          printf("%f, %d, %d, %d\n", .5f*mult, isLock, threadNum, numTimes);  
          my_time += pthread_data(mult, isLock, threadNum);
        }
        my_time = my_time / (float)totalTime;
        printf("%f, %d, %d, %f, %f\n", 1000000000*.5f*mult, isLock, threadNum,my_time, reg_time);
        fprintf(fp,"%f, %d, %d, %f, %f\n", 1000000000*.5f*mult, isLock, threadNum,my_time, reg_time);
      }
    }
  }
  
  fclose(fp);
  return 0;
}
