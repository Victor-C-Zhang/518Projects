#ifndef ASST2_MEMORY_FINDER_H
#define ASST2_MEMORY_FINDER_H

#include <stdint.h>
#include <stddef.h>
#include "direct_mapping.h"
#include "my_pthread_t.h"

/**
 * Swaps data from page at indexA to page at indexB, updates page table and hash table to match the change
 * @param indexA		first page index to be swapped
 * @param indexB		second page index to be swapped
 */
void swap(int indexA, int indexB);

/**
 * Services requests that are within a page or outsources to page_allocate() multiple page allocations
 * @param size			    size of malloc request in bytes
 * @param curr_id		    process making request
 * @param has_allocations    whether the thread already has allocations
 */
void* segment_allocate(size_t size, my_pthread_t curr_id, int has_allocations);

/**
 * Free's allocations
 * @param p			            pointer to free
 * @param curr_id		        process owning the pointer
 * @param has_allocation    whether the thread already has allocations
 */
int free_ptr(void* p, my_pthread_t curr_id, int has_allocation);

/**
 * Sets service ID for segfault checking purposes.
 * @param curr_id
 * @return
 */
my_pthread_t enter_mem_manager(my_pthread_t curr_id);

/**
 * Restores previous ID.
 * @param prev_id
 */
void exit_mem_manager(my_pthread_t prev_id);

#endif //ASST2_MEMORY_FINDER_H
