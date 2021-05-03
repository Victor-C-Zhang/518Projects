#ifndef ASST1_TYPES_H
#define ASST1_TYPES_H
#include "open_address_ht.h"
#include "my_scheduler.h"

#define MEMSIZE 8388608
#define SEGMENTSIZE 64

int MEM_LIMIT;
int VIRT_LIMIT;
int HT_SIZE;

// runtime constants
size_t page_size;  //size of page given to each process
int num_pages; //number of pages available for user
int num_segments; //number of segments within a page available for allocation
uint32_t stack_page_size; // number of pages consumed by a stack
int pages_for_contexts; // number of pages we segregate for contexts
my_pthread_t max_thread_id; // maximum possible thread ID that may be assigned

// metadata/data pointers
char* myblock;

tcb* scheduler_tcb; // static space for scheduler tcb
tcb* main_tcb; // static space for main thread tcb
void* sched_stack_ptr_; // scheduler context stack pointer
ucontext_t* mm_contextarr; // (array) space for context structs to live in

char* mem_space; //user space, after page table
OpenAddrHashtable ht_space; //space where our hashtable starts

// global values for segfault handler
int mm_in_memory_manager; // if we are running an allocation/de-allocation
int mm_curr_id; // thread ID being serviced by the memory manager
my_pthread_t stack_creat_id; // ID of the stack to be created

#endif //ASST1_TYPES_H
