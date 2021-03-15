CC = gcc
CFLAGS = -std=c99 -lrt -g -c
AR = ar -rc
RANLIB = ranlib

Target: my_pthread.a
datastructs:datastructs.c
	$(CC) $(CFLAGS) datastructs -o datastructs.c

my_pthread.a: my_pthread.o
	$(AR) libmy_pthread.a my_pthread.o
	$(RANLIB) libmy_pthread.a

my_pthread.o: my_pthread_t.h
	$(CC) -pthread $(CFLAGS) my_pthread.c

test: datastructs.c pthread_test.c
	$(CC) $(CFLAGS) -Wall datastructs.c
	$(CC) -g -o test pthread_test.c datastructs.c

clean:
	rm -rf testfile *.o *.a
