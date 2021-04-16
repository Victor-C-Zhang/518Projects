CC = gcc
CFLAGS = -std=c99 -g -c
TESTFLAGS = -std=c99 -g -o
AR = ar -rc
RANLIB = ranlib

Target: my_pthread.a
datastructs:datastructs.c
	$(CC) $(CFLAGS) datastructs -o datastructs.c

my_pthread.a: my_pthread.o
	$(AR) libmy_pthread.a my_pthread.o
	$(RANLIB) libmy_pthread.a

my_pthread.o: my_pthread_t.h
	$(CC) -pthread $(CFLAGS) my_pthread.c -lrt -lm

test: datastructs.c my_pthread.c pthread_test.c my_scheduler.c
	$(CC) -g -o test pthread_test.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm

malloc: my_malloc.c datastructs.c my_pthread.c malloc_test.c my_scheduler.c
	$(CC) -g -o malloc my_malloc.c malloc_test.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm

data: datastructs.c my_pthread.c data.c my_scheduler.c
	$(CC) -g -pthread -o data data.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm
data1: datastructs.c my_pthread.c data1.c my_scheduler.c
	$(CC) -g -pthread -o data1 data1.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm
data2: datastructs.c my_pthread.c data2.c my_scheduler.c
	$(CC) -g -pthread -o data2 data2.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm
data3: datastructs.c my_pthread.c data3.c my_scheduler.c
	$(CC) -g -pthread -o data3 data3.c datastructs.c my_pthread.c my_scheduler.c -lrt -lm
clean:
	rm -rf testfile *.o *.a
