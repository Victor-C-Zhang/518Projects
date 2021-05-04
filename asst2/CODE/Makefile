CC      = gcc
CFLAGS  = -std=c99 -pthread -g

CLEANEXTS   = o a

OUTPUTFILE  = libmy_pthread.a
INSTALLDIR  = ./build

# Default target
.PHONY: all
all: $(OUTPUTFILE)

$(OUTPUTFILE): datastructs.o my_scheduler.o my_pthread.o my_malloc.o open_address_ht.o direct_mapping.o memory_finder.o
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
malloc: datastructs.o my_scheduler.o my_pthread.o malloc_test.o my_malloc.o test_runner.o open_address_ht.o direct_mapping.o memory_finder.o
	$(CC) -g -o $@ $^ -lrt -lm

# include dependencies
datastructs.o: datastructs_t.h
my_scheduler.o: my_scheduler.h my_pthread_t.h
my_pthread.o: my_pthread_t.h my_scheduler.h
pthread_test.o: my_pthread_t.h my_scheduler.h
my_malloc.o: my_malloc.h
malloc_test.o: my_malloc.h
test_runner.o: test_runner.h
open_address_ht.o: open_address_ht.h
direct_mapping.o: direct_mapping.h
memory_finder.o: memory_finder.h
