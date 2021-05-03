#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "memory_finder.h"
#include "direct_mapping.h"
#include "open_address_ht.h"
#include "my_pthread_t.h"

/**
 * Swaps data from page at indexA to page at indexB, updates page table and
 * hash table to match the change.
 * Assumes the respective pages are unprotected.
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

// allocates and swaps into the correct location a blank page
void* fetch_blank_page(my_pthread_t curr_id, int position) {
  for (int i = 0; i < num_pages; ++i) {
    pagedata* page = (pagedata*)myblock + i;
    if (!pg_block_occupied(page)) {
      pg_write_pagedata(page, curr_id, OCC, NOT_OVF, position, 1);
      ht_put(ht_space,curr_id,(ht_key)position, (ht_val)i);
      *num_free_pages -= 1;
//      mprotect(mem_space + page_size*position, page_size, PROT_READ | PROT_WRITE);
      if (position != i) {
//        mprotect(mem_space + page_size * i, page_size, PROT_READ | PROT_WRITE);
        swap(position, i);
//        mprotect(mem_space + page_size*i, page_size, PROT_NONE);
      }
      return mem_space + page_size*position;
    }
  }
  // no free pages in memory, return NULL
  // TODO: check for free disk pages
  return NULL;
}

/**
 * Services requests that are within a page or outsources to page_allocate() multiple page allocations
 * @param size			size of malloc request in bytes
 * @param curr_id		process making request
 */
void* segment_allocate(size_t size, my_pthread_t curr_id) {
  // goes from page 0 to end of memory seeing if there's a free space.
  // if so, make sure all pages in the range are allocated, then allocate the
  // memory

  int has_allocation;
  if (curr_id == 0) { //scheduler
    has_allocation = scheduler_tcb->has_allocation;
  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
    has_allocation = main_tcb->has_allocation;
  } else {
    has_allocation = ((tcb*)get(all_threads, curr_id))->has_allocation;
  }

  // nothing has been allocated for this thread yet, so allocate "all of
  // memory" for this thread as one big unused chunk
  if (!has_allocation) {
    if (curr_id == 0) { //scheduler
      scheduler_tcb->has_allocation = 1;
    } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
      main_tcb->has_allocation = 1;
    } else {
      ((tcb*)get(all_threads,curr_id))->has_allocation = 1;
    }
    if (fetch_blank_page(curr_id, 0) == NULL) return NULL;
    dm_write_metadata((metadata*) mem_space, 1, NOT_OCC, LAST);
    pg_write_pagedata((pagedata*) myblock, curr_id, OCC, OVF, 0, num_pages);
  }
  // otherwise we will always be able to find page 0 of memory
  int segments_reqd = ((size+1) % SEGMENTSIZE) ? (size+1)/SEGMENTSIZE + 1 :
                      (size+1)/SEGMENTSIZE;
  int curr_page_num = 0;
  int curr_seg_num = 0;
  int space;
  while (curr_page_num < num_pages) { // try to find a space
    metadata* mdata = (metadata*)mem_space + curr_page_num*page_size +
          curr_seg_num*SEGMENTSIZE;
    pagedata* pdata = (pagedata*)myblock + curr_page_num;
    if (dm_is_last_segment(mdata) && pg_is_overflow(pdata)) {
        space = pdata->length * num_segments + dm_block_size(mdata) - 1; // -1
        // adjustment since segment remainder is [0,64) not (0,64]
    } else {
      space = dm_block_size(mdata);
    }
    if (!dm_block_occupied(mdata) && space >= segments_reqd) { // bingo!
      // ensure all the pages we need are actually allocated
      int last_seg = segments_reqd;
      int pages_to_check = 0;
      last_seg -= num_segments - (curr_seg_num + 1);
      while (last_seg > 0) {
        ++pages_to_check;
        last_seg -= num_segments;
      }

      int next_access_needed =  ((curr_seg_num + segments_reqd + 1)%num_segments == 0 
		      && space > segments_reqd);// will need to access next page as well
      for (int i = 1; i <= pages_to_check; ++i) {
	if (ht_get(ht_space,curr_id,curr_page_num + i) == HT_NULL_VAL) {
      	 	if (next_access_needed && ht_get(ht_space, curr_id, curr_page_num + 
		    	    (curr_seg_num +segments_reqd + 1)/num_segments) == HT_NULL_VAL) {
		       if ( pages_to_check-(i-1)+1 > *num_free_pages) return NULL;
		       break;
		}
		else {
			if (pages_to_check - (i-1) > *num_free_pages) return NULL;
			break;
		}
	}
	if (pages_to_check == i && next_access_needed) {
		if (ht_get(ht_space, curr_id, curr_page_num + (curr_seg_num +
			    segments_reqd + 1)/num_segments) == HT_NULL_VAL) {
			if ( 1 > *num_free_pages) return NULL;	
			break;
		}
	}
      }

      for (int i = 1; i <= pages_to_check; ++i) {
        if (ht_get(ht_space,curr_id,curr_page_num + i) == HT_NULL_VAL)
          //  whole alloc
          if (fetch_blank_page(curr_id,curr_page_num + i) == NULL) {
    		printf("BIG ERROR: NUM FREE PAGES WRONG (1)\n");
		    exit(1);
//		  return NULL;
	  }
      }

      if (next_access_needed) { // will need to access next page as well
        if (ht_get(ht_space, curr_id, curr_page_num + (curr_seg_num +
        segments_reqd + 1)/num_segments) == HT_NULL_VAL) {
          //  whole alloc
          if (fetch_blank_page(curr_id, curr_page_num + (curr_seg_num +
          segments_reqd + 1)/num_segments) == NULL) {
    		printf("BIG ERROR: NUM FREE PAGES WRONG (1)\n");
		    exit(1);
		  //return NULL;
	  }
        }
      }
      printf("malloc num free %d\n", *num_free_pages);
      return dm_allocate_block(curr_id, curr_page_num, curr_seg_num, space,
                     segments_reqd);
    }
    curr_seg_num += space;
    curr_page_num += curr_seg_num / num_segments;
    curr_seg_num %= num_segments;
  }
  // we've gone through all of memory without finding a space
  return NULL;
}

/**
 * Utility function to rewrite the size of a free block. Should only be
 * called from within the context of free_ptr().
 * @param curr_id
 * @param curr_page_num
 * @param curr_seg_num
 * @param mdata
 * @param pdata
 * @param space
 */
void resize_free_block(my_pthread_t curr_id, int curr_page_num, int
curr_seg_num, metadata* mdata, pagedata* pdata, int space) {
  if (curr_seg_num + space < num_segments) { // no overflow, no end of page
    dm_write_metadata(mdata, space, NOT_OCC, NOT_LAST);
  } else if (curr_seg_num + space == num_segments) { // no overflow, end
    // of page
    dm_write_metadata(mdata, space, NOT_OCC, LAST);
  } else { // overflow
    dm_write_metadata(mdata, space%num_segments + 1, NOT_OCC, LAST);
    pg_write_pagedata(pdata, curr_id, OCC, OVF, curr_page_num,
                      space/num_segments);
  }
}

/**
 * Frees allocations and frees pages if possible.
 * @param p			pointer to free
 * @param curr_id		process owning the pointer
 */
int free_ptr(void* p, my_pthread_t curr_id) {
  // search memory for the pointer
  // keep track of previous pointer
  // meld together if necessary
  // if we go past pointer location without finding the pointer, oops

  int has_allocation;
  if (curr_id == 0) { //scheduler
    has_allocation = scheduler_tcb->has_allocation;
  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
    has_allocation = main_tcb->has_allocation;
  } else {
    has_allocation = ((tcb*)get(all_threads, curr_id))->has_allocation;
  }

  // nothing has been allocated for this thread yet, so it must be a bad pointer
  if (!has_allocation) return 2;

  int curr_page_num = 0;
  int curr_seg_num = 0;
  int prev_page_num = -1;
  int prev_seg_num = -1;
  char* pointer = (char*)p - 1;

  if ((pointer-mem_space)%SEGMENTSIZE) return 2; // this should point to the
  // beginning of a block

  while (mem_space + curr_page_num*page_size + curr_seg_num*SEGMENTSIZE < pointer) {
    metadata* mdata = (metadata*)mem_space + curr_page_num*page_size +
          curr_seg_num*SEGMENTSIZE;
    pagedata* pdata = (pagedata*)myblock + curr_page_num;
    prev_seg_num = curr_seg_num;
    prev_page_num = curr_page_num;
    if (dm_is_last_segment(mdata) && pg_is_overflow(pdata)) {
      curr_seg_num += dm_block_size(mdata) - 1;
      curr_page_num += pdata->length + curr_seg_num/num_segments;
      curr_seg_num %= num_segments;
    } else {
      curr_seg_num += dm_block_size(mdata);
      curr_page_num += curr_seg_num/num_segments;
      curr_seg_num %= num_segments;
    }
  }
  if (mem_space + curr_page_num*page_size + curr_seg_num*SEGMENTSIZE ==
  pointer) { // we found it!
    metadata* mdata = (metadata *) pointer;
    pagedata* pdata = (pagedata*)myblock + curr_page_num;
    if (!dm_block_occupied(mdata)) return 1; // double free

    // it's good!
    dm_write_metadata(mdata, dm_block_size(mdata), NOT_OCC,
                      dm_is_last_segment(mdata));
    int space;
    if (dm_is_last_segment(mdata) && pg_is_overflow(pdata)) {
      space = pdata->length*num_segments + dm_block_size(mdata) - 1;
    } else {
      space = dm_block_size(mdata);
    }
    // check if we need to meld with next
    int next_page_num;
    int next_seg_num;
    if (dm_is_last_segment(mdata) && pg_is_overflow(pdata)) {
      next_seg_num = curr_seg_num + dm_block_size(mdata) - 1;
      next_page_num = curr_page_num + pdata->length + next_seg_num/num_segments;
      next_seg_num %= num_segments;
    } else {
      next_seg_num = curr_seg_num + dm_block_size(mdata);
      next_page_num = curr_page_num + next_seg_num/num_segments;
      next_seg_num %= num_segments;
    }
    metadata* next_mdata = (metadata*)mem_space + next_page_num*page_size +
          next_seg_num*SEGMENTSIZE;
    pagedata* next_pdata = (pagedata*)myblock + next_page_num;
    if (!dm_block_occupied(next_mdata)) { // meld
      if (dm_is_last_segment(next_mdata) && pg_is_overflow(next_pdata)) {
        space += next_pdata->length*num_segments + dm_block_size(next_mdata)
              - 1;
      } else {
        space += dm_block_size(next_mdata);
      }
      resize_free_block(curr_id, curr_page_num, curr_seg_num, mdata, pdata,
                        space);
    }
    int free_seg_num;
    int free_page_num;
    if (prev_seg_num != -1) { // meld with prev if necessary
      metadata* prev_mdata = (metadata*)mem_space + prev_page_num*page_size
            + prev_seg_num*SEGMENTSIZE;
      pagedata* prev_pdata = (pagedata*)myblock + prev_page_num;
      if (!dm_block_occupied(prev_mdata)) {
        free_seg_num = prev_seg_num;
        free_page_num = prev_page_num;
        if (dm_is_last_segment(prev_mdata) && pg_is_overflow(prev_pdata)) {
          space += prev_pdata->length*num_segments + dm_block_size(prev_mdata)
                   - 1;
        } else {
          space += dm_block_size(prev_mdata);
        }
        resize_free_block(curr_id, prev_page_num, prev_seg_num, prev_mdata,
                          prev_pdata, space);
      } else {
        free_seg_num = curr_seg_num;
        free_page_num = curr_page_num;
      }
    } else {
      free_seg_num = curr_seg_num;
      free_page_num = curr_page_num;
    }
    space -= num_segments - free_seg_num;
    ++free_page_num; // cannot de-alloc first page no matter what we do
    while (space >= num_segments) {
      ht_val actual = ht_get(ht_space, curr_id, free_page_num);
      if (actual != HT_NULL_VAL) {
        pg_write_pagedata((pagedata*)myblock + actual, -1, NOT_OCC,
                          NOT_OVF, -1, 1);
        ht_delete(ht_space, curr_id, (ht_key)free_page_num);
        *num_free_pages += 1;
      }
      ++free_page_num;
      space -= num_segments;
    }
    printf("free num free %d\n", *num_free_pages);
    return 0;
  } else { // nooooo we didn't find it
    return 2;
  }
}
