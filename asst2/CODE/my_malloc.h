#ifndef ASST1_MY_MALLOC_H
#define ASST1_MY_MALLOC_H

#include <stddef.h>
#include <string.h>
#include "my_scheduler.h" 

#define THREADREQ 1
#define LIBRARYREQ 0
//#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
//#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)

struct _pagedata_ {
	uint32_t pid; //id of process using page
	unsigned short p_ind; //where process thinks data is; left most bit == 1, is occupied, else free
//	unsigned short swapped_to; //where prior data was swapped to
//		don't need this now that we have hashtable ??
} typedef pagedata;

void* myallocate(size_t size, char* file, int line, int threadreq);
void mydeallocate(void* p, char* file, int line, int threadreq);
void printMemory();
#endif //ASST1_MY_MALLOC_H
