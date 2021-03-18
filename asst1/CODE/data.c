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

struct timeval start;
struct timeval stop;
int totalTime = 20;
FILE *fp;

void* my_thread_func(void* args) {
  my_params* vals = (my_params*)args;
  long long n = (long long) 1000000000 * vals->multiple;
  int isLock = vals->isLock;
  my_pthread_mutex_t* lock  = (my_pthread_mutex_t*)vals->lock;
  if (isLock)  my_pthread_mutex_lock(lock);
  while (n--) { }
  if (isLock)  my_pthread_mutex_unlock(lock);
  return (void*)30;
}


void* thread_func(void* args) {
  params* vals = (params*)args;
  long long n = (long long) 1000000000 * vals->multiple;
  int isLock = vals->isLock;
  pthread_mutex_t* lock  = (pthread_mutex_t*)vals->lock;
  if (isLock)  pthread_mutex_lock(lock);
  while (n--) { }
  if (isLock)  pthread_mutex_unlock(lock);
  return (void*)30;
}

void my_pthread_data() {
	fp=fopen("my_pthread_data.csv","w+");	
	fprintf(fp,"Multiple, Synchronized, Number of threads, Avg time across %d runs\n", totalTime);
	for (int mult = 1; mult <= 8; mult++){  
		for (int isLock = 0; isLock <= 1; isLock++) {
			for (int threadNum = 1; threadNum <= 10; threadNum++) {
				float time = 0; 
				for (int numTimes = 1; numTimes <= totalTime; numTimes++) {
					gettimeofday(&start, 0);
					my_pthread_t threads[threadNum];
					my_params* args[threadNum];
					my_pthread_mutex_t lock;
					my_pthread_mutex_init(&lock, NULL);
					for (int i = 0; i < threadNum; i++){
						args[i] = (my_params*) malloc(sizeof(my_params));
						args[i] -> lock = &lock;
						args[i] -> multiple = mult * .25f;
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

					gettimeofday(&stop, 0);
					time += (( stop.tv_sec-start.tv_sec)*1000000 + stop.tv_usec-start.tv_usec);
				}
				time = time / (float)totalTime;
				fprintf(fp,"%f, %d, %d, %f\n", .25f*mult, isLock, threadNum,time);
			}
		}
	}
	
	fclose(fp);
}

void pthread_data() {
	fp=fopen("pthread_data.csv","w+");	
	fprintf(fp,"Multiple, Synchronized, Number of threads, Avg time across %d runs\n", totalTime);
	for (int mult = 1; mult <= 8; mult++){  
		for (int isLock = 0; isLock <= 1; isLock++) {
			for (int threadNum = 1; threadNum <= 10; threadNum++) {
				float time = 0; 
				for (int numTimes = 1; numTimes <= totalTime; numTimes++) {
					gettimeofday(&start, 0);
					pthread_t threads[threadNum];
					pthread_attr_t threadAttrs;
					pthread_attr_init(&threadAttrs);
					params* args[threadNum];
					pthread_mutex_t lock;
					pthread_mutex_init(&lock, NULL);
					for (int i = 0; i < threadNum; i++){
						args[i] = (params*) malloc(sizeof(params));
						args[i] -> lock = &lock;
						args[i] -> multiple = mult * .25f;
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

					gettimeofday(&stop, 0);
					time += (( stop.tv_sec-start.tv_sec)*1000000 + stop.tv_usec-start.tv_usec);
				}
				time = time / (float)totalTime;
				fprintf(fp,"%f, %d, %d, %f\n", .25f*mult, isLock, threadNum,time);
			}
		}
	}	
	fclose(fp);
}

int main(int argc, char** argv){
	pthread_data();
	my_pthread_data();
	return 0;
}
