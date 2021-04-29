#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "memory_finder.h"
#include "direct_mapping.h"
#include "open_address_ht.h"
#include "my_pthread_t.h"
/**
 * Swaps data from page at indexA to page at indexB, updates page table and hash table to match the change
 * @param indexA		first page index to be swapped
 * @param indexB		second page index to be swapped
 */
void swap(int indexA, int indexB){
	if (indexA == indexB) {return;}
	pagedata* pdA = (pagedata*)myblock+indexA;
	pagedata* pdB = (pagedata*)myblock+indexB;
	ht_put(ht_space, pdA->pid, (ht_key)pg_index(pdA), indexB);
	ht_put(ht_space, pdB->pid, (ht_key)pg_index(pdB), indexA);
	pagedata pdTemp;
	pg_write_pagedata(&pdTemp, pdA->pid, pg_block_occupied(pdA), pg_is_overflow(pdA), pg_index(pdA), pdA->length);
	pg_write_pagedata(pdA, pdB->pid, pg_block_occupied(pdB), pg_is_overflow(pdB), pg_index(pdB), pdB->length);
	pg_write_pagedata(pdB, pdTemp.pid, pg_block_occupied(&pdTemp), pg_is_overflow(&pdTemp), pg_index(&pdTemp), pdTemp.length);
	char* pageA = (char*)mem_space + indexA*page_size;
	char* pageB = (char*)mem_space + indexB*page_size;
	char  pageTemp;
	size_t i;
	for (i=0; i<page_size; i++) {
		pageTemp = pageA[i];
		pageA[i] = pageB[i];
		pageB[i] = pageTemp;
	}
}

/**
 * Finds the start and end of curr_id's view of memory, first free page, and number of free pages
 * @param curr_id		ID of process in question.
 * @param free_page		pointer to store index of first free page
 * @param num_free 		pointer to store number of free pages available
 * @param first_page_index	pointer to store the first page index according to process' view
 * @param last_page_index	pointer to store the last page index according to process' view
 */
int find_contig_space(my_pthread_t curr_id, int* free_page, int* num_free_pages, int* first_page_index, int* last_page_index){
	*free_page = -1;
	*num_free_pages = 0;
	// change
	if (curr_id == 0) { //scheduler
	  *first_page_index = scheduler_tcb->first_page_index;
	  *last_page_index = scheduler_tcb->last_page_index;
	} else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
	  *first_page_index = main_tcb->first_page_index;
	  *last_page_index = main_tcb->last_page_index;
	} else {
    *first_page_index = ((tcb*)get(all_threads,curr_id))->first_page_index;
    *last_page_index = ((tcb*)get(all_threads,curr_id))->last_page_index;
  }
	for (int index = 0; index < num_pages; index++) {
		pagedata* pdata = (pagedata*)myblock + index;
//		printf("find_cont page[%d], isocc %d currid %d\n", index, pg_block_occupied(pdata), pdata->pid);
		if ( !pg_block_occupied(pdata) ) { //free page
			if (*free_page == -1) *free_page = index;
			*num_free_pages=*num_free_pages+1;
		}
	}
}

/**
 * Services malloc requests which require new or contiguous page allocations
 * @param size			size of malloc request in bytes
 * @param curr_id		process making request
 * @param free_page 		first free page index
 * @param page_index		index of process' page from which the allocation will start
 * @param seg_index		index of page's segment from which the allocation will start
 */
void* page_allocate(size_t size, my_pthread_t curr_id, int free_page, int page_index, int seg_index) {
	int pages_alloc = (size / page_size) + 1;

	if ( (page_index != -1 && page_index+pages_alloc >= num_pages) ||
		(page_index == -1 && free_page+pages_alloc >= num_pages) ) {
		return NULL;
	}

	int process_index; //index where process thinks the page is
	int actual_index; //index where page actually is
	int ret_seg_index = 0; //segment index of return pointer
	int swap_array_index = 0; //counter for array of pages to swap
	uint8_t ovf_len = (size % page_size) / SEGMENTSIZE + 1; //segment count in the last segment of contiguous page allocation
	//there are no pages allocated for the process yet, so process will think its contiguous memory starts at the first free page
	if (page_index == -1) {
	  process_index = free_page;
	  tcb* thread_tcb;
	  if (curr_id == 0) { // scheduler
	    thread_tcb = scheduler_tcb;
	  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
	    thread_tcb = main_tcb;
	  } else {
	    thread_tcb = get(all_threads,curr_id);
	  }
	  thread_tcb->first_page_index = thread_tcb->last_page_index = free_page;
	}
	//if there is memory allocated for the process, check if the page_index's last segment is free
	else {
		if (seg_index != -1){
			actual_index = ht_get(ht_space, curr_id, (ht_key)page_index);
			metadata* curr = (metadata*) (mem_space + actual_index*page_size + seg_index*SEGMENTSIZE);
			//if last segment is free, set segment's occupied flag and the overflow length
			//and set the page to be an overflow with correct page length
			if (!dm_block_occupied(curr)){
//				printf("last page last seg %d\n", seg_index);
//				printf("page segment mallc\n");
				ret_seg_index = seg_index;
				size -= (num_segments-seg_index)*SEGMENTSIZE;
				ovf_len = (size % page_size)/SEGMENTSIZE +1;

				// there's something wrong with this arithmetic but I don't have the
				// energy to figure this out right now
				pages_alloc = (size / page_size)+1;
				dm_write_metadata(curr,ovf_len,OCC,LAST);
				pagedata* pdata = (pagedata*)myblock+actual_index;
				pg_write_pagedata(pdata, curr_id, OCC, OVF, pg_index(pdata), pages_alloc+1);
				swap_array_index++;
			}
		}
		//since the process has memory allocated for it already, any additional pages allocated
		//for the process must seem contiguous, so need to know where process thinks
		//its current last page is, so subsequent pages could be appropriately swapped
		process_index=page_index+1;
	}

	//array for swapping pages to correct index after all pages allocated
	int swap_len = (swap_array_index == 1) ? pages_alloc+1 : pages_alloc;
	int pages_actual_index[swap_len];
	if (swap_array_index == 1) pages_actual_index[0] = actual_index;

	//if free_page is -1, then we are not allocating new pages, but within the process' existing contiguous memory space
	actual_index = (free_page == -1) ? process_index : free_page;
	//allocates additional pages
	while (pages_alloc && actual_index < num_pages) {
		if (free_page == -1) actual_index = ht_get(ht_space, curr_id, (ht_key)process_index);
		pagedata* pdata = (pagedata*)myblock + actual_index;
		metadata* start = (metadata*) (mem_space + actual_index*page_size);
		//update HT, PT, and metadata is page is allocated to process
		if ( !pg_block_occupied(pdata) || pdata->pid == curr_id ) {
			//add the actual index and where process thinks the page is to HT
			ht_put(ht_space, curr_id, (ht_val)process_index, actual_index);
			// extend first_index, last_index if necessary
      tcb* thread_tcb;
      if (curr_id == 0) { // scheduler
        thread_tcb = scheduler_tcb;
      } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
        thread_tcb = main_tcb;
      } else {
        thread_tcb = get(all_threads,curr_id);
      }
      if (thread_tcb->first_page_index > process_index)
        thread_tcb->first_page_index = process_index;
      if (thread_tcb->last_page_index < process_index)
        thread_tcb->last_page_index = process_index;
			//if last page to alloc, set metadata for the free segment following the overflow segment
			if (pages_alloc == 1 && ovf_len != num_segments && swap_array_index != 0){
				int curr_seg = dm_block_size(start);
				dm_write_metadata(start+ovf_len*SEGMENTSIZE,curr_seg-ovf_len, NOT_OCC, dm_is_last_segment(start));
			}
			//if first page in allocation set page data and set segment data of the last
			if (swap_array_index == 0) {
//				printf("page mallc\n");
				int is_ovf = (pages_alloc == 1) ? NOT_OVF : OVF;
				pg_write_pagedata(pdata, curr_id, OCC, is_ovf, process_index, pages_alloc);
				dm_allocate_block(start,ovf_len);
			}
			else { 	//if not 1st page, write pagedata for process owning page and where process thinks it is,
				pg_write_pagedata(pdata, curr_id, OCC, NOT_OVF, process_index, 1);
			}
			//add the free page allocated into array for swapping to correct location later
			pages_actual_index[swap_array_index] = actual_index;
			swap_array_index++;
			pages_alloc--;
			process_index++;
		}

		actual_index++;
	}
	//swap newly allocated pages to where the process thinks they are
	//and set return pointer to be returned from first page newly allocated
	void* ret;
	for (int i = 0; i < swap_len; i++){
		pagedata* p = (pagedata*)myblock+pages_actual_index[i];
		if (i == 0) {
			ret = mem_space+pg_index(p)*page_size+ret_seg_index*SEGMENTSIZE+1;
		}
		swap(pages_actual_index[i], pg_index(p) );
	}

	return ret;
}

/**
 * Services requests that are within a page or outsources to page_allocate() multiple page allocations
 * @param size			size of malloc request in bytes
 * @param curr_id		process making request
 */
void* segment_allocate(size_t size, my_pthread_t curr_id){
	int first_page_index;
	int last_page_index;
	int free_page;
	int num_free;
	find_contig_space(curr_id, &free_page, &num_free, &first_page_index, &last_page_index);
	size_t segments_alloc = size / SEGMENTSIZE + 1;
	int pages_alloc = (segments_alloc/num_segments+1);

	int alloc_page_index; //page index according the process where an allocation could start
	int alloc_seg_index = -1; //segment index within alloc_page_index page where allocation could start
	int contig_free_space = 0; //from start index, to current index how much contiguous free space available
	int process_index = first_page_index;
	uint8_t ovf_len = 0;
	//traverse memory to find the most current current contigious free block that can fit the size requested
	while (process_index <= last_page_index) {
		int actual_index = ht_get(ht_space, curr_id, (ht_key)process_index);
		pagedata* pdata = (pagedata*)myblock + actual_index;
		metadata* start = (metadata*) (mem_space + actual_index*page_size);
		int seg_index = ovf_len;
		while ( seg_index < num_segments) {
			metadata* curr = start + seg_index*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(curr);
			if (dm_is_last_segment(curr)) {
				if (pg_is_overflow(pdata)) ovf_len=curr_seg;
				else ovf_len = 0;
				curr_seg = num_segments - seg_index;
				if (last_page_index == process_index) {
	//				printf("last page last seg %d\n", seg_index);
					alloc_seg_index = seg_index;
				}
			}

			if (!dm_block_occupied(curr)){
				if (contig_free_space == 0) {
					alloc_page_index = process_index;
					alloc_seg_index = seg_index;
				}
				contig_free_space+=curr_seg;
				if (contig_free_space >= segments_alloc){
					//current segment can service the request
					if (curr_seg >= segments_alloc) {
						dm_allocate_block(curr, segments_alloc);
						swap(actual_index, process_index);
						metadata* swapped_mdata = (metadata*) (mem_space+process_index*page_size) + seg_index*SEGMENTSIZE;
//						printf("segment mallc\n");
						return (void*) (swapped_mdata + 1);
					}
					//multiple page allocation required --> outsource
					//free_page = -1 to convey that this is
					//memory allocation within existing spac
//					printf("contig page alloc\n");
					free_page = -1;
					break;
				}
			}
			else {
				contig_free_space = 0;
				alloc_page_index = -1;
//				alloc_seg_index = -1;
			}
			seg_index+=curr_seg;
		}

		if (contig_free_space >= segments_alloc) break;
		else if (ovf_len) process_index+=(pdata->length-1);
		else { process_index++; }
	}

	//if we need to allocate new pages, make sure there's enough space
	if ( free_page != -1) {
		if (pages_alloc > num_free) return NULL;
	//	printf("free page new page\n");
		alloc_page_index = last_page_index;
	}
	return page_allocate(size, curr_id, free_page, alloc_page_index, alloc_seg_index);
}

/**
 * Releases pages from process' contiguous space if they are free and at front or end of memory space
 * @param curr_id		process id
 * @param first_page_index	index of first page in process' view of contiguous memory space
 * @param last_page_index	index of last page in process' view of contiguous memory space
 */
void free_process_pages(my_pthread_t curr_id, int first_page_index, int last_page_index){
	int free_start = first_page_index;
	int process_index = first_page_index;
	int ovf_len = 0;
	while (process_index <= last_page_index) {
		ht_val actual_index = (uint16_t) ht_get(ht_space,curr_id,(ht_key)(process_index));
		pagedata* pdata = (pagedata*)myblock+actual_index;
		metadata* start = (metadata*) (mem_space + actual_index*page_size);
		int seg_index = ovf_len;
		while ( seg_index < num_segments) {
			metadata* curr = start + seg_index*SEGMENTSIZE;
			int curr_seg = dm_block_size(curr);
			int is_last = dm_is_last_segment(curr);
			if (is_last) {
				if (pg_is_overflow(pdata)) ovf_len=curr_seg;
				else ovf_len = 0;
				curr_seg = num_segments - seg_index;
			}
			if (seg_index == 0 && !dm_block_occupied(curr) && is_last) {
				if (free_start == first_page_index){
					pg_write_pagedata(pdata, curr_id, NOT_OCC, NOT_OVF, process_index, 1);
          if (curr_id == 0) { //scheduler
            scheduler_tcb->first_page_index = process_index + 1;
          } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
            main_tcb->first_page_index = process_index + 1;
          } else {
            ((tcb*)get(all_threads,curr_id))->first_page_index = process_index + 1;
          }
					ht_delete(ht_space, curr_id, process_index);
				}
				else if (free_start == -1) {
					free_start = process_index;
				}
			}
			if (dm_block_occupied(curr)){
				free_start = -1;
			}

			seg_index+=curr_seg;

		}
		if (ovf_len) process_index+=(pdata->length-1);
		else { process_index++; }
	}

	if (free_start != -1 && free_start != first_page_index) {
    if (curr_id == 0) { //scheduler
      scheduler_tcb->last_page_index = free_start - 1;
    } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
      main_tcb->last_page_index = free_start - 1;
    } else {
      ((tcb*)get(all_threads,curr_id))->last_page_index = free_start - 1;
    }
		for (process_index = free_start; process_index <= last_page_index; process_index++){
			ht_val actual_index = (uint16_t) ht_get(ht_space,curr_id,(ht_key)(process_index));
			pagedata* pdata = (pagedata*)myblock+actual_index;
			metadata* start = (metadata*) (mem_space + actual_index*page_size);
			pg_write_pagedata(pdata, curr_id, NOT_OCC, NOT_OVF, process_index, 1);
			ht_delete(ht_space, curr_id, process_index);
		}
	}

	// if we've released all alloc-ed memory, update first/last indices to
	// reflect this
  if (curr_id == 0) { //scheduler
    if (scheduler_tcb->first_page_index > scheduler_tcb->last_page_index) {
      scheduler_tcb->first_page_index = UINT16_MAX;
      scheduler_tcb->last_page_index = -1;
    }
  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
    if (main_tcb->first_page_index > main_tcb->last_page_index) {
      main_tcb->first_page_index = UINT16_MAX;
      main_tcb->last_page_index = -1;
    }
  } else {
    tcb* thread = (tcb*)get(all_threads,curr_id);
    if (thread->first_page_index > thread->last_page_index) {
      thread->first_page_index = UINT16_MAX;
      thread->last_page_index = -1;
    }
  }
}

/**
 * Free's allocations that span multiple pages
 * @param curr			segment where the allocation starts
 * @param pdata			page data for page where the allocation is
 */
void free_cont_pages(metadata* curr, pagedata* pdata) {
	uint8_t ovf_len = dm_block_size(curr);
	uint16_t num_contig_pages = pdata->length;

	//change first page of multiple page allocation to not overflow, and length of page allocation to be 1
	pg_write_pagedata(pdata, pdata->pid, OCC, NOT_OVF, pg_index(pdata), 1);

	//find the last page in the multiple page allocation,
	//set the beginning of the page to a free block the
	//size of the overflow length
	int p_index = pg_index(pdata);
	ht_val last_alloc_page_index = (uint16_t) ht_get(ht_space,pdata->pid,(ht_key)(p_index+num_contig_pages-1));
	pagedata* last = (pagedata*)myblock+last_alloc_page_index;
	curr = (metadata*) mem_space+last_alloc_page_index*page_size;
	if (ovf_len < num_segments){ //consolidate free segments within the last page
		dm_write_metadata(curr, ovf_len, NOT_OCC,NOT_LAST);
		metadata* next = curr + ovf_len*SEGMENTSIZE;
		if (!dm_block_occupied(next)) dm_write_metadata(curr, ovf_len + dm_block_size(next), NOT_OCC, dm_is_last_segment(next));
	}
	else dm_write_metadata(curr, ovf_len,NOT_OCC,LAST);
}

/**
 * Free's allocations
 * @param p			pointer to free
 * @param curr_id		process owning the pointer
 */
int free_ptr(void* p, my_pthread_t curr_id) {
	//find the process' contiguous memory space
	int first_page_index;
	int last_page_index;
	int free_page;
	int num_free;
	find_contig_space(curr_id, &free_page, &num_free, &first_page_index, &last_page_index);
//	printf("here0: first %d last %d\n", first_page_index, last_page_index);
	//page where pointer is according to process
	int page_index = ( (char*)p - 1 - mem_space) / page_size ;
	//if nothing has been malloc'd yet, cannot free!
	int process_index = first_page_index; //index of page in contig memory according to process
	uint8_t ovf_len = 0;

	//find the pointer to be free'd
	//keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (process_index <= last_page_index){
//		printf("here\n");
		int actual_index = ht_get(ht_space, curr_id, (ht_key)process_index);
		if (process_index == page_index) {
			swap(process_index, actual_index);
			actual_index = process_index;
		}
		pagedata* pdata = (pagedata*)myblock + actual_index;
		metadata* prev;
		int prevFree = 0;
		uint8_t prevSize = 0;
		//traverse page to find segment of pointer to free
		metadata* start = (metadata*) (mem_space + actual_index*page_size);
		int seg_index = ovf_len;
		while (seg_index < num_segments) {
//			printf("here1\n");
			metadata* curr = start + seg_index*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(curr);
			if (dm_is_last_segment(curr)) curr_seg = num_segments - seg_index;
			//if we find the pointer to be free'd
			if ( curr + 1 == p) {
				//if it is already free, cannot free it -> error
				if ( !dm_block_occupied(curr) ) return 1;
				else {
					int num_contig=1;
					// free current pointer and any next segments that might be free
					if (dm_is_last_segment(curr)){
						//free the last segment in first page of multiple page allocation
					        if (pg_is_overflow(pdata)) {
							num_contig = pdata->length;
							free_cont_pages(curr, pdata);
						}
						dm_write_metadata(curr, num_segments-seg_index, NOT_OCC, LAST);
					}
					else {
						dm_write_metadata(curr, curr_seg, NOT_OCC, dm_is_last_segment(curr));
						//if the next block is within segment and is free, combine if with my current pointer as a free block
						if (seg_index+curr_seg < num_segments){
							metadata* next = curr + curr_seg*SEGMENTSIZE;
							if (!dm_block_occupied(next)){
								dm_write_metadata(curr, curr_seg + dm_block_size(next), NOT_OCC, dm_is_last_segment(next));
							}
						}
					}

					//if previous block was free, combine my current pointer with previous block
					if (prevFree) dm_write_metadata(prev, prevSize + dm_block_size(curr), NOT_OCC, dm_is_last_segment(curr));
					//if the page of the pointer to free is first page of process' contiguous memory space,
					//and the page is now empty, free all pages up to a occupied page

					//free pages if possible
					free_process_pages(curr_id, first_page_index, last_page_index);
					return 0;
				}

			}

/*			printf("free: page[%d][%d]\tpid %d\tp_ind %d\t ovf %d\t ovflen %d: %p %d %d %hu\n",
					actual_index, seg_index, pdata->pid, pg_index(pdata), pg_is_overflow(pdata), pdata->length, curr, dm_block_occupied(curr), dm_is_last_segment(curr), curr_seg);
*/			if (dm_is_last_segment(curr)) {
				if (pg_is_overflow(pdata)) ovf_len=dm_block_size(curr);
				else ovf_len=0;
			}

			prev = curr;
			if (!dm_block_occupied(curr)) {
				prevFree = 1;
				prevSize = curr_seg;
			}
			else {
				prevFree = 0;
			}
			seg_index += curr_seg;
		}

		if (ovf_len) process_index+=(pdata->length-1);
		else {
			process_index++;
			ovf_len=0;
		}
	}
	//if we did not return within the while loop, the pointer was not found and thus was not malloc'd
	return 2;
}

