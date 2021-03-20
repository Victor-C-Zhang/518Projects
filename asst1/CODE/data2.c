// File:  my_pthread.c
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "my_pthread_t.h"
#include <pthread.h>

typedef struct params {
	float multiple;
	int isLock; //1 is yes
	pthread_mutex_t* lock;
} params;

typedef struct my_params {
	float multiple;
	int isLock; //1 is yes
	my_pthread_mutex_t* lock;
} my_params;

FILE *fp;
clock_t start;
clock_t stop;
int totalTime = 10;
int multMin = 2;
int multTot = 2;
int lockMin = 0;
int lockTot = 0;
int threadMin = 1;
int threadTot = 15;
char* fName = "data2.csv";

void startTime(){ 
	start = clock();	
}

void endTime(){ 
	stop = clock();
}

float durTime(){
	float dur = ( (float) (stop - start) )/CLOCKS_PER_SEC;
//	float dur = ( (float) (stop - start) );
	return dur;
}

void* my_thread_func(void* args) {

  my_params* vals = (my_params*)args;
  long long n = (long long) 1000000000 * vals->multiple;
  int isLock = vals->isLock;
  my_pthread_mutex_t* lock  = (my_pthread_mutex_t*)vals->lock;
  if (isLock)  my_pthread_mutex_lock(lock);
//  printf("while\n");
  while (n--) { 
 //	if (!(n%5000000)) printf("Thread: %lld\n", n);
  }
  if (isLock)  my_pthread_mutex_unlock(lock);
  return (void*)30;
}


void* thread_func(void* args) {
  params* vals = (params*)args;
  long long n = (long long) 1000000000 * vals->multiple;
  int isLock = vals->isLock;
  pthread_mutex_t* lock  = (pthread_mutex_t*)vals->lock;
  if (isLock)  pthread_mutex_lock(lock);
//  printf("while\n");
  while (n--) {
// 	if (!(n%5000000)) printf("Thread: %lld\n", n);
  }
  if (isLock)  pthread_mutex_unlock(lock);
  return (void*)30;
}

float my_pthread_data(int mult, int isLock, int threadNum) {
//	printf(".5f*mult, isLock, threadNum\n");	
//	printf("%f, %d, %d\n", .5f*mult, isLock, threadNum);	
	startTime();
	my_pthread_t threads[threadNum];
	my_params* args[threadNum];
	my_pthread_mutex_t lock;
	my_pthread_mutex_init(&lock, NULL);
	for (int i = 0; i < threadNum; i++){
		args[i] = (my_params*) malloc(sizeof(my_params));
		args[i] -> lock = &lock;
		args[i] -> multiple = mult * .5f;
		args[i] -> isLock = isLock;
		my_pthread_create(&threads[i], NULL, my_thread_func, (void*)args[i]);
	}
	for (int i = 0; i < threadNum; i++) {
		my_pthread_join(threads[i], NULL);
	}	
	for (int i = 0; i < threadNum; i++) {
		free(args[i]);
	}
	my_pthread_mutex_destroy(&lock);
	endTime();
	return durTime();
}

float pthread_data(int mult, int isLock, int threadNum) {
//	printf(".5f*mult, isLock, threadNum\n");	
//	printf("%f, %d, %d\n", .5f*mult, isLock, threadNum);	
	startTime();
	pthread_t threads[threadNum];
	pthread_attr_t threadAttrs;
	pthread_attr_init(&threadAttrs);
	params* args[threadNum];
	pthread_mutex_t lock;
	pthread_mutex_init(&lock, NULL);
	for (int i = 0; i < threadNum; i++){
		args[i] = (params*) malloc(sizeof(params));
		args[i] -> lock = &lock;
		args[i] -> multiple = mult * .5f;
		args[i] -> isLock = isLock;
		pthread_create(&threads[i], &threadAttrs, thread_func, (void*)args[i]);
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

int test() {
	my_pthread_mutex_t lock;
	my_pthread_mutex_init(&lock, NULL);
	my_pthread_t other;
	my_params* args = (my_params*) malloc(sizeof(my_params));
	args -> lock = &lock;
	args -> multiple = 4 * .5f;
	args -> isLock = 0;
	my_pthread_create(&other, NULL, my_thread_func, (void*)args);
	long long n = 1000000000;
	while (n--) {
		if (!(n%5000000)) printf("Main: %lld\n",n);
	}

	my_pthread_mutex_destroy(&lock);
}

int main(int argc, char** argv){
	fp=fopen(fName,"w+");	
	fprintf(fp,"Multiple, Synchronized, Thread Num, my_pthread Avg Time, Reg pthread Avg Time\n");
	for (int mult = multTot; mult >= multMin; mult--){  
		for (int isLock = lockMin; isLock <= lockTot; isLock++) {
			for (int threadNum = threadMin; threadNum <= threadTot; threadNum++) {
				float my_time = 0; 
				float reg_time = 0; 
				for (int numTimes = 1; numTimes <= totalTime; numTimes++) {
					printf("%f, %d, %d, %d\n", .5f*mult, isLock, threadNum, numTimes);	
//					reg_time += pthread_data(mult, isLock, threadNum);
					my_time += my_pthread_data(mult, isLock, threadNum);
				}
				my_time = my_time / (float)totalTime;
//				reg_time = reg_time / (float)totalTime;
				printf("%f, %d, %d, %f, %f\n", 1000000000*.5f*mult, isLock, threadNum,my_time, reg_time);
//				fprintf(fp,"%f, %d, %d, %f, %f\n", 1000000000*.5f*mult, isLock, threadNum,my_time, reg_time);
			}
		}
	}
	
	fclose(fp);
/*	mult = 4;
	isLock = 0;
	threadNum = 10;
	pthread_data();
	my_pthread_data();
	test();	*/
	return 0;
}
