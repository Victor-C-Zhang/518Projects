#ifndef ASST2_MEMORY_FINDER_H
#define ASST2_MEMORY_FINDER_H

#include <stdint.h>
#include <stddef.h>
#include "direct_mapping.h"

/**
 * Swaps data from page at indexA to page at indexB, updates page table and hash table to match the change
 * @param indexA		first page index to be swapped
 * @param indexB		second page index to be swapped
 */
void swap(int indexA, int indexB);

/**
 * Finds the start and end of curr_id's view of memory, first free page, and number of free pages
 * @param curr_id		ID of process in question.
 * @param free_page		pointer to store index of first free page
 * @param num_free 		pointer to store number of free pages available
 * @param first_page_index	pointer to store the first page index according to process' view
 * @param last_page_index	pointer to store the last page index according to process' view
 */
int find_contig_space(uint32_t curr_id, int* free_page, int* num_free_pages, int* first_page_index, int* last_page_index);

/**
 * Services malloc requests which require new or contiguous page allocations
 * @param size			size of malloc request in bytes
 * @param curr_id		process making request
 * @param free_page 		first free page index
 * @param page_index		index of process' page from which the allocation will start
 * @param seg_index		index of page's segment from which the allocation will start
 */
void* page_allocate(size_t size, uint32_t curr_id, int free_page, int page_index, int seg_index);

/**
 * Services requests that are within a page or outsources to page_allocate() multiple page allocations
 * @param size			size of malloc request in bytes
 * @param curr_id		process making request
 */
void* segment_allocate(size_t size, uint32_t curr_id);

/**
 * Releases pages from process' contiguous space if they are free and at front or end of memory space
 * @param curr_id		process id
 * @param first_page_index	index of first page in process' view of contiguous memory space
 * @param last_page_index	index of last page in process' view of contiguous memory space
 */
void free_process_pages(uint32_t curr_id, int first_page_index, int last_page_index);

/**
 * Free's allocations that span multiple pages
 * @param curr			segment where the allocation starts
 * @param pdata			page data for page where the allocation is
 */
void free_cont_pages(metadata* curr, pagedata* pdata);

/**
 * Free's allocations
 * @param p			pointer to free
 * @param curr_id		process owning the pointer
 */
int free_ptr(void* p, uint32_t curr_id);

#endif //ASST2_MEMORY_FINDER_H
