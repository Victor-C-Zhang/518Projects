#ifndef ASST1_MY_MALLOC_H
#define ASST1_MY_MALLOC_H

#include <stddef.h>
#include "my_scheduler.h" 

#define THREADREQ 1
#define LIBRARYREQ 0
#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)
#define MEMSIZE 8388608
#define NUMSEGMENTS 64

//metadata is represented as an unsigned char
//it tells us the amount of bytes (following the metadata block) 
//which are allocated for  user data
//if the leftmost bit is 1, then the block is occupied
//else it is free
struct _metadata_ {
	unsigned char size;
} typedef metadata;

struct _pagedata_ {
	uint32_t pid; //-1 if free, else id of process using data
	unsigned short p_ind; //where process thinks data is
	unsigned short swapped_to; //where prior data was swapped to
} typedef pagedata;

void* myallocate(size_t size, char* file, int line, int threadreq);
void mydeallocate(void* p, char* file, int line, int threadreq);
void printMemory();
#endif //ASST1_MY_MALLOC_H
