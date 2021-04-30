#ifndef ASST1_TYPES_H
#define ASST1_TYPES_H
#include "open_address_ht.h"
#include "my_scheduler.h"

#define MEMSIZE 8388608
#define SEGMENTSIZE 64

int MEM_LIMIT;
int VIRT_LIMIT;
int HT_SIZE;

void* sched_stack_ptr_; // scheduler context stack pointer
size_t page_size;  //size of page given to each process
int num_pages; //number of pages available for user
uint32_t stack_page_size; // number of pages consumed by a stack
int num_segments; //number of segments within a page available for allocation

char* myblock;
tcb* scheduler_tcb; // static space for scheduler tcb
tcb* main_tcb; // static space for main thread tcb
OpenAddrHashtable ht_space; //space where our hashtable starts
char* mem_space; //user space, after page table


#endif //ASST1_TYPES_H
