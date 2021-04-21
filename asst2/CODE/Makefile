CC      = gcc
CFLAGS  = -std=c99 -pthread

CLEANEXTS   = o a

OUTPUTFILE  = libmy_pthread.a
INSTALLDIR  = ./build

# Default target
.PHONY: all
all: $(OUTPUTFILE)

$(OUTPUTFILE): datastructs.o my_scheduler.o my_pthread.o my_malloc.o
	ar ru $@ $^
	ranlib $@

.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(OUTPUTFILE) $(INSTALLDIR)

.PHONY: clean
clean:
	rm -rf testfile *.o *.a
	for file in $(CLEANEXTS); do rm -f *.$$file; done

tests: datastructs.o my_scheduler.o my_pthread.o pthread_test.o
	$(CC) -g -o $@ $^ -lrt -lm
malloc: datastructs.o my_scheduler.o my_pthread.o malloc_test.o my_malloc.o
	$(CC) -g -o $@ $^ -lrt -lm

# include dependencies
datastructs.o: datastructs_t.h
my_scheduler.o: my_scheduler.h my_pthread_t.h
my_pthread.o: my_pthread_t.h my_scheduler.h
pthread_test.o: my_pthread_t.h my_scheduler.h
my_malloc.o: my_malloc.h
malloc_test.o: my_malloc.h
