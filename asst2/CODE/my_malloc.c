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
OpenAddrHashtable ht_space; //space where our hashtable starts 

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
//		if (pdata->pid == 0) {continue;}
		metadata* start = (metadata*) ((char*)mem_space + i*page_size);
		printf("--------------------PAGE--------------------\n");
		int j = 0;
		while (j < num_segments) {
			metadata* mdata = start + j*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(mdata);
			int isOcc = dm_block_occupied(mdata);
			int isLast = dm_is_last_segment(mdata);
			printf("page[%d][%d]\tpid %d\tp_ind %d\t ovf %d\t ovflen %d: %p %d %d %hu\n",
					i, j, pdata->pid, pg_index(pdata), pg_is_overflow(pdata), pdata->length, mdata, isOcc, isLast, curr_seg);
			j+=curr_seg;	
			if (isLast) j = num_segments;
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
//makes allocations that will overflow into next page
void* contiguous_page_alloc(size_t size, uint32_t curr_id, int free_page, int last_page_ind, int last_seg_ind) {
	int pages_alloc = (size / page_size) + 1;
	if ( (last_page_ind != -1 && last_page_ind+pages_alloc >= num_pages) ||
		(last_page_ind == -1 && free_page+pages_alloc >= num_pages) ) {
		return NULL;
	}

	int proc_ind; //index where process thinks the page is
	int ret_seg_index = 0; //segment index of return ptr 
	int arr_ind = 0; //counter for array of pages to swap 
	uint8_t ovf_len = (size % page_size) / SEGMENTSIZE + 1; //len of allocation in the last segment of contiguous page allocation
	int actual_index; 

	//there are no pages allocated for the process yet, so process will think 
	//its contiguous memory starts at the first free page
	if (last_page_ind == -1) proc_ind = free_page; 
	//if there is memory allocated for the process, check if the last
	//page of the contiguous memory has a last segment that is free
	else {
		actual_index = ht_get(ht_space, curr_id, (ht_key)last_page_ind);
		pagedata* pdata = (pagedata*)myblock+actual_index;
		metadata* curr = (metadata*) (mem_space + actual_index*page_size + last_seg_ind*SEGMENTSIZE);
		//if last segment is free, set it to occupied, set the overflow length
		//and set the page to be an overflow
		if (!dm_block_occupied(curr)){
			ret_seg_index = last_seg_ind;
			size -= (num_segments-last_seg_ind)*SEGMENTSIZE;
			ovf_len = (size % page_size)/SEGMENTSIZE +1;
			pages_alloc = (size / page_size)+1;
			dm_write_metadata(curr,ovf_len,OCC,LAST);
			pg_write_pagedata(pdata, curr_id, OCC, OVF, pg_index(pdata), pages_alloc+1);
			arr_ind++;
		}
		
		//since the process has memory allocated for it already, any additional pages allocated 
		//for the process must seem contiguous, so need to know where process thinks
		//its current last page is, so subsequent pages could be appropriately swapped		
		proc_ind=last_page_ind+1;
	}
	
	//array for swapping pages to correct index after free pages 
	//are found and allocated 
	int swap_len = (arr_ind == 1) ? pages_alloc+1 : pages_alloc;
	int pages_actual_index[swap_len];
	if (arr_ind == 1) pages_actual_index[0] = actual_index;
	int index = (free_page == -1) ? proc_ind : free_page; 
	while (pages_alloc && index < num_pages) {
		if (free_page == -1) index = ht_get(ht_space, curr_id, (ht_key)proc_ind);
		pagedata* pdata = (pagedata*)myblock + index;
		metadata* start = (metadata*) (mem_space + index*page_size);
		//update HT, PT, and metadata is page is allocated to process
		if ( !pg_block_occupied(pdata) || pdata->pid == curr_id ) {
			//add the actual index and where process thinks the page is to HT
			ht_put(ht_space, curr_id, (ht_val)proc_ind, index);
			//if this is the first page in the contiguous allocation
			//set page to: occupied, overflowing, and length of pages in allocation
			//also, set last segment of first page to hold the overflow length
			if (arr_ind == 0) {
				int is_ovf = (pages_alloc == 1) ? NOT_OVF : OVF;
				pg_write_pagedata(pdata, curr_id, OCC, is_ovf, proc_ind, pages_alloc);
				dm_allocate_block(start,ovf_len);
			}
			//if not first page, simply write pagedata for who the page belongs to,
			//and where the process thinks the page is
			else {
				pg_write_pagedata(pdata, curr_id, OCC, NOT_OVF, proc_ind, 1);
			}
			//if this is the last page to allocate, set metadata for the free segment 
			//that follows the overflow segment
			if (pages_alloc == 1 && ovf_len != num_segments && arr_ind != 0){
				dm_write_metadata(start+ovf_len*SEGMENTSIZE,num_segments-ovf_len, NOT_OCC, LAST);
			}
			//add the free page allocated into array for swapping 
			//to correct location later
			pages_actual_index[arr_ind] = index;
			arr_ind++;
			pages_alloc--;
			proc_ind++;
		}
		
		index++;
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

void* find_free_segment(uint32_t curr_id, size_t size, int first_page_ind, int last_page_ind, int free_page, int num_free){
	size_t segments_alloc = size / SEGMENTSIZE + 1;
	int pages_alloc = (segments_alloc/num_segments+1);

	int start_index; //index according the process where tan allocation could start 
	int last_seg_ind = -1;
	int contig_free_space = 0; //from start index to current index how much contiguous free space available
	int proc_ind = first_page_ind;
	uint8_t ovf_len = 0;

	while (proc_ind <= last_page_ind){
		int actual_location = ht_get(ht_space, curr_id, (ht_key)proc_ind);
		pagedata* pdata = (pagedata*)myblock + actual_location;
		//traverse memory to find the first free block that can fit the size requested
		metadata* start = (metadata*) (mem_space + actual_location*page_size);
		int seg_index = ovf_len;
		while ( seg_index < num_segments) {
			metadata* curr = start + seg_index*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(curr);
			if (dm_is_last_segment(curr)) {
				if (pg_is_overflow(pdata)) { 
					ovf_len=curr_seg;
				}
				else { ovf_len=0; }
				curr_seg = num_segments - seg_index;
				if (last_page_ind == proc_ind) last_seg_ind = seg_index;
			}
			if (!dm_block_occupied(curr)){
				if (contig_free_space == 0) {
					start_index = proc_ind;
					last_seg_ind = seg_index;
				}
				contig_free_space+=curr_seg;
				if (contig_free_space >= segments_alloc){
					if (curr_seg >= segments_alloc) {
						dm_allocate_block(curr, segments_alloc);
						swap(actual_location, proc_ind);
						metadata* swapped_mdata = (metadata*) (mem_space+proc_ind*page_size) + seg_index*SEGMENTSIZE;
						return (void*) (swapped_mdata + 1);
					}
					last_page_ind = start_index;
					free_page = -1;
					break;

				}
			}
			else {
				contig_free_space = 0;
				start_index = -1;
			}
			seg_index+=curr_seg;
		}
	
		if (contig_free_space >= segments_alloc) break;
		else if (ovf_len) proc_ind+=(pdata->length-1);
		else { proc_ind++; }
	}

	if ( free_page != -1 && pages_alloc > num_free) return NULL; 
	return contiguous_page_alloc(size, curr_id, free_page, last_page_ind, last_seg_ind);
}

void* find_free_page(size_t size, uint32_t curr_id) {
	int first_page_ind = INT16_MAX;
	int last_page_ind = -1;
	int free_page = -1;
	int num_free_pages = 0;

	//traverse memory to find the contiguous memory space of current process
	//if exists, and find first free page, as well as the number of free pages
	for (int index = 0; index < num_pages; index++) {	
		pagedata* pdata = (pagedata*)myblock + index;	
		if ( !pg_block_occupied(pdata) ) { //free page
			if (free_page == -1) free_page = index;
			num_free_pages++;
		}
		else if (pdata->pid == curr_id) { //occupied page see if it belongs to current process
			//find the start and end of the process' contiguous space
			int ind_acc_proc = pg_index(pdata);//index of current page according to process...
			last_page_ind = (ind_acc_proc > last_page_ind) ? ind_acc_proc : last_page_ind;
			first_page_ind = (ind_acc_proc < first_page_ind) ? ind_acc_proc : first_page_ind;
		}
	}

	void* val = find_free_segment(curr_id, size, first_page_ind, last_page_ind, free_page, num_free_pages);
	return val;
}

//returns to the user a pointer to the amount of memory requested
void* myallocate(size_t size, char* file, int line, int threadreq){
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
		ht_space = (ht_entry*) (mem_space + page_size*num_pages);
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

//free's ptr that spans across multiple pages
void free_cont_pages(metadata* curr, pagedata* pdata, int seg_index, int* last_page_ind) {
	uint8_t ovf_len = dm_block_size(curr);
	uint16_t num_contig_pages = pdata->length;
	
	//change first page of multiple page allocation to not overflow, and length of page allocation to be 1
	pg_write_pagedata(pdata, pdata->pid, OCC, NOT_OVF, pg_index(pdata), 1);

	//find the last page in the multiple page allocation, set the beginning of the page to a free block the 
	//size of the overflow length 
	int p_index = pg_index(pdata);
	ht_val last_page_index = (uint16_t) ht_get(ht_space,pdata->pid,(ht_key)(p_index+num_contig_pages-1));
	pagedata* last = (pagedata*)myblock+last_page_index;
	curr = (metadata*) mem_space+last_page_index*page_size;
	
	//consolidate free segments within the last page
	if (ovf_len < num_segments){
		dm_write_metadata(curr, ovf_len,NOT_OCC,NOT_LAST);
		metadata* next = curr + ovf_len*SEGMENTSIZE;
		if (!dm_block_occupied(next)) dm_write_metadata(curr, ovf_len + dm_block_size(next), NOT_OCC, dm_is_last_segment(next));	
	}
	else dm_write_metadata(curr, ovf_len,NOT_OCC,LAST);
	
	//if the last page in multiple page allocation is the last page in the process' contiguous memory space
	//deallocate pages from process
	if (dm_is_last_segment(curr) && (num_contig_pages-1+p_index == *last_page_ind)) {
		for (int i = 1; i < num_contig_pages; i++) {
			ht_val next_page_index = (uint16_t) ht_get(ht_space,pdata->pid,(ht_key)(p_index+i));
			pagedata* next = (pagedata*)myblock+next_page_index;
			pg_write_pagedata(next, next->pid, NOT_OCC, NOT_OVF, p_index+i, 1);
			ht_delete(ht_space, next->pid, p_index+i);
		}
		*last_page_ind = p_index;
	}
}

int find_contig_space(uint32_t curr_id, int* first_page_ind, int* last_page_ind){ 
	*first_page_ind = INT16_MAX;
	*last_page_ind = -1;
	//traverse memory to find the contiguous memory space of current process
	for (int index = 0; index < num_pages; index++) {	
		pagedata* pdata = (pagedata*)myblock + index;	
		if ( pg_block_occupied(pdata) && (pdata->pid == curr_id) ) { //occupied page see if it belongs to current process
			//find the start and end of the process' contiguous space
			int ind_acc_proc = pg_index(pdata);//index of current page according to process...
			*last_page_ind = (ind_acc_proc > *last_page_ind) ? ind_acc_proc : *last_page_ind;
			*first_page_ind = (ind_acc_proc < *first_page_ind) ? ind_acc_proc : *first_page_ind;
		}
	}
	
}

int free_ptr(void* p, uint32_t curr_id) {
	//process thinks the page is, swap it where with it actually is
	int first_page_ind;
	int last_page_ind;
	find_contig_space(curr_id, &first_page_ind, &last_page_ind);

	int page_index = ( (char*)p - 1 - mem_space) / page_size ;
	//if nothing has been malloc'd yet, cannot free!
	int proc_ind = (firstMalloc == 1) ? last_page_ind+1 : first_page_ind;
	uint8_t ovf_len = 0;

	//find the pointer to be free and keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (proc_ind <= last_page_ind){
		int actual_location = ht_get(ht_space, curr_id, (ht_key)proc_ind);
		if (proc_ind == page_index) {
			swap(proc_ind, actual_location);
			actual_location = proc_ind;
		}
		
		pagedata* pdata = (pagedata*)myblock + actual_location;
		metadata* prev;
		int prevFree = 0;
		uint8_t prevSize = 0;
		//traverse memory to find the first free block that can fit the size requested
		metadata* start = (metadata*) (mem_space + actual_location*page_size);
		int seg_index = ovf_len;
		while (seg_index < num_segments) { 
			metadata* curr = start + seg_index*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(curr);
			if (dm_is_last_segment(curr)) {
				curr_seg = num_segments - seg_index;
			}
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
							free_cont_pages(curr, pdata, seg_index, &last_page_ind);
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
	
					//can't free pages if the page is all free, there might be pages before or after that are allocated

					if (!dm_block_occupied(start) && dm_is_last_segment(start) && ovf_len == 0 && page_index == first_page_ind) {	
						//last page free only if last not occ and last segment
						for (int i = 0; i < num_contig; i++) {
							ht_val next_page_index = (uint16_t) ht_get(ht_space,pdata->pid,(ht_key)(page_index+i));
							pagedata* next = (pagedata*)myblock+next_page_index;
							if (i == num_contig-1){
								curr = (metadata*)mem_space+next_page_index*page_size;
								if( dm_block_occupied(curr) || !dm_is_last_segment(curr)){
									break;
								}
							}
							pg_write_pagedata(next, next->pid, NOT_OCC, NOT_OVF, page_index+i, 1);	
							ht_delete(ht_space, next->pid, page_index+i);
						}
					}
					
					return 0;
				}
			}
		
			if (dm_is_last_segment(curr)) {
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
		
		if (ovf_len) proc_ind+=(pdata->length-1);
		else {
			proc_ind++;
			ovf_len=0;
		}
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
	
	uint32_t curr_id = (threadreq == LIBRARYREQ) ? 0 : ( (tcb*) get_head(ready_q[curr_prio]) )->id;
	int d = free_ptr(p, curr_id);
	if (d == 1) {
		error_message("Cannot free an already free pointer!", file, line);
	}
	else if (d==2) {
		error_message("This pointer was not malloc'd!", file, line);
	}
	exit_scheduler(&timer_pause_dump);
}
