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
	$(CC) -pthread $(CFLAGS) my_pthread.c -lrt

test: datastructs.c my_pthread.c pthread_test.c my_scheduler.c
	$(CC) -g -o test pthread_test.c datastructs.c my_pthread.c my_scheduler.c -lrt

data: datastructs.c my_pthread.c data.c my_scheduler.c
	$(CC) -g -o data data.c datastructs.c my_pthread.c my_scheduler.c -lrt

clean:
	rm -rf testfile *.o *.a
