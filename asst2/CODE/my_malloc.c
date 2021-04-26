#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "direct_mapping.h"
#include "open_address_ht.h"

static char* myblock;
static int firstMalloc = 1;
struct sigaction segh;
size_t page_size;  //size of page given to each process
int num_pages; //number of pages available for user
int num_segments; //number of segments within a page available for allocation 
char* mem_space; //user space, after page table
OpenAddrHashtable ht_space;

static void handler(int sig, siginfo_t* si, void* unused) {
	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
	exit(0);
}

//prints any error messages followed by memory 
void error_message(char* error, char* file, int line) {
	printf("%s:%d: %s\n", file, line, error);
	exit(0);
}

//for debugging and error reporting purposes
//prints memory as index of memory
//followed by 0 for free or any other integer for occupied
//followed by the size of user data
void printMemory() {
	printf("<----------------------MEMORY---------------------->\n");
	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		if (!pg_block_occupied(pdata)) {continue;}
		metadata* start = (metadata*) ((char*)mem_space + i*page_size);
		printf("--------------------PAGE--------------------\n");
		int j = 0;
		while (j < num_segments) {
			metadata* mdata = start + j*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(mdata);
			int isOcc = dm_block_occupied(mdata);
			int isLast = dm_is_last_segment(mdata);
			printf("page[%d][%d]\t\tpid %d\t%p: %d %d %hu\n",
					i, j, pdata->pid, mdata, isOcc, isLast, curr_seg);
			j+=curr_seg;	
		}
	}
	printf("</---------------------MEMORY---------------------->\n");
}

void initialize_pages() {
	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		pg_write_pagedata(pdata, 0, NOT_OCC, NOT_OVF, i, 1);
		metadata* mdata = (metadata*) ((uint8_t*)mem_space + i*page_size);
		dm_write_metadata(mdata, num_segments, 0, 1);
	}
}

//swaps data from page at indexA to page at indexB
//updates the inverted pagetable and hastable to match
//change in location of data
void swap(int indexA, int indexB){
	if (indexA == indexB) {return;}

	pagedata* pdA = (pagedata*)myblock+indexA;
	pagedata* pdB = (pagedata*)myblock+indexB;

	ht_val valA = ht_get(ht_space, pdA->pid, (ht_key)pg_index(pdA)); 
	ht_val valB = ht_get(ht_space, pdB->pid, (ht_key)pg_index(pdB)); 
	ht_put(ht_space, pdA->pid, (ht_key)pg_index(pdA), valB);
	ht_put(ht_space, pdB->pid, (ht_key)pg_index(pdB), valA);

	pagedata pdTemp;
	pg_write_pagedata(&pdTemp, pdA->pid, pg_block_occupied(pdA), pg_is_overflow(pdA), pg_index(pdA), pdA->length);
	pg_write_pagedata(pdA, pdB->pid, pg_block_occupied(pdB), pg_is_overflow(pdB), pg_index(pdB), pdB->length);
	pg_write_pagedata(pdB, pdTemp.pid, pg_block_occupied(&pdTemp), pg_is_overflow(&pdTemp), pg_index(&pdTemp), pdTemp.length);
	
	char pageTemp[page_size];
	char* pageA = (char*)mem_space + indexA*page_size;
	char* pageB = (char*)mem_space + indexB*page_size;
	memcpy(pageA, pageTemp, page_size);
	memcpy(pageB, pageA, page_size);
	memcpy(pageTemp, pageB, page_size);
}

//makes allocations that will overflow into the next page 
void* contiguous_page_alloc(size_t segments_alloc, int pages_alloc, uint32_t curr_id, int free_page, int last_page_ind, int last_seg_ind) {
	int proc_ind; //index where process thinks the page is
	int ret_seg_index = 0; //segment index of return ptr 
	int arr_ind = 0; //counter for array of pages to swap 
	//len of allocation in the last segment of contiguous page allocation
	uint8_t ovf_len = segments_alloc % num_segments;
	
	if (last_page_ind == -1) proc_ind = free_page;
	else {
		pagedata* pdata = (pagedata*)myblock+last_page_ind;
		metadata* start = (metadata*) (mem_space + last_page_ind*page_size);
		metadata* curr = start;
		if (last_seg_index != -1) curr = start+last_seg_index*SEGMENTSIZE;
		else {
			last_seg_index = 0;
			while ( last_seg_index < num_segments && !dm_is_last_segment(curr)) {
				last_seg_index+=dm_block_size(curr);
				curr = start + last_segment_index*SEGMENTSIZE;
			}
		}
		
		if (!dm_block_occupied(curr)){
			ret_seg_index = last_seg_index;
			segments_alloc-=(num_segments-last_seg_index);
			ovf_len = segments_alloc%num_segments+1;
			pages_alloc = segments_alloc/num_segments+1;
			dm_write_metadata(curr,ovf_len,OCC,LAST);
			pg_write_pagedata(pdata, curr_id, OCC, OVF, pg_index(pdata), pages_alloc+1);
			arr_ind++;
		}
		
		proc_ind=pg_index(pdata)+1;
	}
	
	int swap_len = (arr_ind == 1) ? pages_alloc+1 : pages_alloc;
	int pages_actual_index[swap_len];
	if (arr_ind == 1) pages_actual_index[0] = last_page_ind;
	int index = free_page;
	while (pages_alloc && index < num_pages) {
		pagedata* pdata = (pagedata*)myblock + index;	
		metadata* start = (metadata*) (mem_space + index*page_size);
		if ( !pg_block_occupied(pdata) ) { 
			ht_put(ht_space, curr_id, (ht_val)proc_ind, index);
			if (arr_ind == 0) {
				pg_write_pagedata(pdata, curr_id, OCC, OVF, proc_ind, pages_alloc);
				dm_write_metadata(start,segments_alloc % num_segments,OCC,LAST);
			}
			else {
				pg_write_pagedata(pdata, curr_id, OCC, NOT_OVF, proc_ind, 1);
			}
			if (pages_alloc == 1) {
				dm_write_metadata(start+ovf_len*SEGMENTSIZE,num_segments-ovf_len, NOT_OCC, LAST);
			}

			pages_actual_index[arr_ind] = index;
			arr_ind++;
			pages_alloc--;
			proc_ind++;
		}
		index++;
	}

	void* ret;
	for (int i = 0; i < swap_len; i++){
		pagedata* p = (pagedata*)myblock+i;
		swap(pages_actual_index, pg_index(p) );
		if (i == 0) {
			ret = mem_space+pg_index(p)*page_size+ret_seg_index*SEGMENTSIZE+1;
		}
	}

	return ret;
}

void* find_free_page(size_t size, uint32_t curr_id) {
	//find page for process...
	size_t segments_alloc = size / SEGMENTSIZE + 1;
	int pages_alloc = (segments_alloc)/num_segments+1;
	int free_page = -1;
	int num_free_pages = 0;
	int last_page_ind = -1;
	int last_seg_ind = -1;
	int index = 0;

	//if exists, find first fit free space
	for (; index < num_pages; index++) {	
		pagedata* pdata = (pagedata*)myblock + index;	
		if ( !pg_block_occupied(pdata) ) { //free page
			if (free_page == -1) free_page = index;
			num_free_pages++;
		}
		else if (pdata->pid == curr_id) { //occupied page
			int ind_acc_proc = pg_index(pdata);//index of current page according to process...
			//if this page is later in the contiguous memory space according to process, update last_page_index...
			last_page_ind = (ind_acc_proc > last_page_ind) ? ind_acc_proc : last_page_ind;
			
			if (segments_alloc < num_segments){ 
				//traverse memory to find the first free block that can fit the size requested
				metadata* start = (metadata*) (mem_space + index*page_size);
				int seg_index = 0;
				uint8_t ovf_len = -1;
				while ( seg_index < num_segments) {
					metadata* curr = start + seg_index*SEGMENTSIZE;
					uint8_t curr_seg = dm_block_size(curr);
					if (dm_is_last_segment(curr)) {
						curr_seg = num_segments - seg_index;
						if (pg_is_overflow(pdata)) ovf_len = dm_block_size(curr);
						if (last_page_ind == ind_acc_proc) *last_seg_ind = seg_index;	
					}
					if (!dm_block_occupied(curr) && (segments_alloc <= curr_seg ) ) {
						dm_allocate_block(curr, segments_alloc);
						//if this is the last segment allocated, it was initially free, therefore cannot be an overflow
						//free last segments == no overflow! 
/*						if (ovf_len != -1) { 
							metadata* next;
							if (segments_alloc == curr_seg) {
								ht_val next_act = (uint16_t) ht_get(ht_space,curr_id, (ht_key)(ind_acc_proc+1));
								pagedata* next = (pagedata*)myblock+next_act;
								uint16_t nextlen = pdata->length-1;
								int next_ovf = (nextlen > 1) ? OVF : NOT_OVF;
								pg_write_pagedata(pdata, curr_id, OCC, NOT_OVF, pg_index(pdata), 1);
								pg_write_pagedata(next, curr_id,pg_block_occupied(next), next_ovf, pg_index(next),nextlen);
								next = (metadata*) (mem_space + next_act*page_size);
							}
							else {
								next = curr + segments_alloc*SEGMENTSIZE;
							}	
							dm_write_metadata(next,ovf_len,NOT_OCC,LAST);
						}			
*/						swap(index, ind_acc_proc);
						metadata* swapped_mdata = (metadata*) (mem_space+ind_acc_proc*page_size) + seg_index*SEGMENTSIZE;
						return (void*) (swapped_mdata + 1);
					}
					seg_index+=curr_seg;
				}
			}	
		}
		//else not enough space, find any other pages allocated to pid
	}

	if ( (pages_alloc > num_free_pages) || 
			(last_page_ind != -1 && last_page_ind+pages_alloc >= num_pages) ||
			(last_page_ind == -1 && free_page+pages_alloc >= num_pages) ) {
		return NULL; 
	}

	return contiguous_page_alloc(segments_alloc, pages_alloc, curr_id, free_page, last_page_ind, last_seg_ind);
}

//returns to the user a pointer to the amount of memory requested
void* myallocate(size_t size, char* file, int line, int threadreq){
	enter_scheduler(&timer_pause_dump);
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
			}
			if (pages_alloc == 1) {
				dm_write_metadata(start+ovf_len*SEGMENTSIZE,num-segments-ovf_len, NOT_OCC, LAST);
			}
			pages_alloc--;

		}
		index++;
	}
	return NULL;
}

//returns to the user a pointer to the amount of memory requested
void* myallocate(size_t size, char* file, int line, int threadreq) {
	enter_scheduler(&timer_pause_dump);
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		error_message("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}
	uint32_t curr_id = (threadreq == LIBRARYREQ) ? 0 : ( (tcb*) get_head(ready_q[curr_prio]) )->id;
	if (firstMalloc == 1) { // first time using malloc
		myblock = memalign(sysconf( _SC_PAGESIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGESIZE);
		num_segments = page_size / SEGMENTSIZE;
		num_pages = MEMSIZE / ( page_size + sizeof(pagedata) + sizeof(ht_entry) );
		uint32_t pt_space = num_pages*sizeof(pagedata);
		pt_space = (pt_space % page_size) ? pt_space/page_size + 1 : pt_space/page_size;
		mem_space = myblock + pt_space*page_size;
		OpenAddrHashtable ht_space = (ht_entry*) (memspace + page_size*num_pages);
		createTable(ht_space);

		initialize_pages();
		memset(&segh, 0, sizeof(struct sigaction));
		sigemptyset(&segh.sa_mask);
		segh.sa_sigaction = handler;
		segh.sa_flags = SA_SIGINFO;
		sigaddset(&segh.sa_mask,SIGALRM); // block scheduling attempts while resolving memory issues
		sigaction(SIGSEGV, &segh, NULL);
		firstMalloc = 0;
	}

	void* p = find_free_page(size, curr_id); 
	if (p == NULL) {
		error_message("Not enough memory", file, line);
	}
  	exit_scheduler(&timer_pause_dump);
	return p; 
}

int free_cont_pages(metadata* curr, pagedata* pdata, int seg_index) {
	uint8_t ovf_len = dm_block_size(curr);
	dm_write_metadata(curr, num_segments-seg_index, NOT_OCC, LAST);
	pg_write_pagedata(pdata, pdata->pid, OCC, NOT_OVF, pg_index(pdata), 1);
	uint16_t num_contig_pages = pdata->length;
	int p_index = pg_index(pdata);
	for (int i = 1; i < num_contig_pages; i++) {
		ht_val next_page_index = (uint16_t) ht_get(ht_space,pdata->curr_id,(ht_key)(p_index+i));
		pagedata* next = (pagedata*)myblock+next_page_index;
		if (i+1 == num_contig_pages){
			metadata* curr = (metadata*) mem_space+next_page_index*page_size;
			if (ovf_len < num_segments){
				dm_write_metadata(curr, ovf_len,NOT_OCC,NOT_LAST);
				metadata* next = curr + ovf_len*SEGMENTSIZE;
				if (!dm_block_occupied(next)){
					dm_write_metadata(curr, ovf_len + dm_block_size(next), NOT_OCC, dm_is_last_segment(next));
				}
			}
			else { dm_write_metadata(curr, ovf_len,NOT_OCC, LAST); }
		}
		else {
			pg_write_pagedata(next, next->pid, OCC, NOT_OVF, pg_index(next), 1);
		}
	}
}

int free_ptr(void* p) {
	int page_index = ( (char*)p - 1 - mem_space) / page_size ;
	pagedata* pdata = (pagedata*)myblock+page_index;
	metadata* start = (metadata*) (mem_space + page_index*page_size);
	metadata* prev;
	int prevFree = 0;
	uint8_t prevSize = 0;
	//if nothing has been malloc'd yet, cannot free!
	int seg_index = (firstMalloc == 1) ? num_segments: 0;
	//find the pointer to be free and keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (seg_index < num_segments) { 
		metadata* curr = start + seg_index*SEGMENTSIZE;
		uint8_t curr_seg = dm_block_size(curr);
		//if we find the pointer to be free'd
		if ( curr + 1 == p) {
			//if it is already free, cannot free it -> error
			if ( !dm_block_occupied(curr) ) {
				return 1;
			}
			else {
				// free current pointer
				if (dm_is_last_segment(curr)){
				       if (pg_is_overflow(pdata)) free_cont_pages(curr, pdata, seg_index);
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

				//can't free pages if the page is all free, there might be pages before or after that are allocated
				if (!dm_block_occupied(start) && dm_block_size(start) == num_segments) {
					//check if this is first or last pagedata block associated w/ the pid...how? thooo --> error ?
					int p_index = pg_index(pdata);
					if (p_index + 1 < num_pages && ht_get(ht_space,pdata->curr_id,(ht_key)(p_index+1)) == HT_NULL_VALL) {
						pg_write_pagedata( pdata, pdata->pid, NOT_OCC, NOT_OVF, page_index, 1);	
					}
					else if ( (p_index > 0 ) && (ht_get(ht_space,pdata->curr_id,(ht_key)(p_index-1)) == HT_NULL_VAL)) {
						pg_write_pagedata( pdata, pdata->pid, NOT_OCC, NOT_OVF, page_index, 1);	
							
					}
				}
				
			}		
			return 0;
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

	//if we did not return within the while loop, the pointer was not found and thus was not malloc'd
	return 2;
}

//frees up any memory malloc'd with this pointer for future use
//frees up any memory malloc'd with this pointer for future use
void mydeallocate(void* p, char* file, int line, int threadreq) {
	enter_scheduler(&timer_pause_dump);
	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		error_message("Can't free null pointer!", file, line);
		return;
	}
	
	int d = free_ptr(p);
	if (d == 1) {
		error_message("Cannot free an already free pointer!", file, line);
	}
	else if (d==2) {
		error_message("This pointer was not malloc'd!", file, line);
	}
	exit_scheduler(&timer_pause_dump);
}
