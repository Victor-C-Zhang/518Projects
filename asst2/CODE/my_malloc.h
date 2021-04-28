#ifndef ASST1_MY_MALLOC_H
#define ASST1_MY_MALLOC_H
#include <stddef.h>
#include <string.h>
#include "my_scheduler.h" 
#include "open_address_ht.h"

#define THREADREQ 1
#define LIBRARYREQ 0
#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)

static char* myblock;
size_t page_size;  //size of page given to each process
int num_pages; //number of pages available for user
int num_segments; //number of segments within a page available for allocation
char* mem_space; //user space, after page table
OpenAddrHashtable ht_space; //space where our hashtable starts 

void* myallocate(size_t size, char* file, int line, int threadreq);
void mydeallocate(void* p, char* file, int line, int threadreq);
void printMemory();
#endif //ASST1_MY_MALLOC_H
