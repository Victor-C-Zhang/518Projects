#ifndef ASST1_MY_MALLOC_H
#define ASST1_MY_MALLOC_H
#include <stddef.h>
#include <string.h>

#define THREADREQ 1
#define LIBRARYREQ 0
#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)

void* myallocate(size_t size, char* file, int line, int threadreq);
void mydeallocate(void* p, char* file, int line, int threadreq);
void printMemory();
#endif //ASST1_MY_MALLOC_H
