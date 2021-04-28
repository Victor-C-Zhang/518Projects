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
	int ovf_len = 0;
	int i = 0;
	while (i < num_pages){
//	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		int j = ovf_len;
		if (!pg_block_occupied(pdata)) { 
			i++; 
			continue; 
		}
//		if (pdata->pid == 0) {continue;}
		metadata* start = (metadata*) ((char*)mem_space + i*page_size);
		printf("--------------------PAGE--------------------\n");
		while (j < num_segments) {
			metadata* mdata = start + j*SEGMENTSIZE;
			uint8_t curr_seg = dm_block_size(mdata);
			int isOcc = dm_block_occupied(mdata);
			int isLast = dm_is_last_segment(mdata);
			printf("page[%d][%d]\tpid %d\tp_ind %d\t ovf %d\t ovflen %d: %p %d %d %hu\n",
					i, j, pdata->pid, pg_index(pdata), pg_is_overflow(pdata), pdata->length, mdata, isOcc, isLast, curr_seg);
			j+=curr_seg;	
			if (isLast) {
				j = num_segments;
				if (pg_is_overflow(pdata)) ovf_len=curr_seg;
				else ovf_len = 0;
			}
		}
		if (ovf_len) i+=(pdata->length-1);
		else { i++; }
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
int find_contig_space(uint32_t curr_id, int* free_page, int* num_free_pages, int* first_page_index, int* last_page_index){ 
	*free_page = -1;
	*num_free_pages = 0;
	*first_page_index = INT16_MAX;
	*last_page_index = -1;
	for (int index = 0; index < num_pages; index++) {	
		pagedata* pdata = (pagedata*)myblock + index;	
//		printf("find_cont page[%d], isocc %d currid %d\n", index, pg_block_occupied(pdata), pdata->pid);
		if ( !pg_block_occupied(pdata) ) { //free page
			if (*free_page == -1) *free_page = index;
			*num_free_pages=*num_free_pages+1;
		}
		else if (pdata->pid == curr_id) { //occupied page, so see if it belongs to current process
			int ind_acc_proc = pg_index(pdata); //index of current page according to process...
			
			*last_page_index = (ind_acc_proc > *last_page_index) ? ind_acc_proc : *last_page_index;
			*first_page_index = (ind_acc_proc < *first_page_index) ? ind_acc_proc : *first_page_index;
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
void* page_allocate(size_t size, uint32_t curr_id, int free_page, int page_index, int seg_index) {
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
	if (page_index == -1) process_index = free_page; 
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
void* segment_allocate(size_t size, uint32_t curr_id){
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
	
	void* p = segment_allocate(size, curr_id);
//	printf("malloc %d call %s:%d %p\n", size / SEGMENTSIZE+1, file, line, p);
//	printMemory();
	if (p == NULL) {
		error_message("Not enough memory", file, line);
	}
  	exit_scheduler(&timer_pause_dump);
	return p;
}


void free_process_pages(uint32_t curr_id, int first_page_index, int last_page_index){
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
		for (process_index = free_start; process_index <= last_page_index; process_index++){
			ht_val actual_index = (uint16_t) ht_get(ht_space,curr_id,(ht_key)(process_index));
			pagedata* pdata = (pagedata*)myblock+actual_index;
			metadata* start = (metadata*) (mem_space + actual_index*page_size);
			pg_write_pagedata(pdata, curr_id, NOT_OCC, NOT_OVF, process_index, 1);	
			ht_delete(ht_space, curr_id, process_index);
		}
	}
}

/**
 * Free's allocations that span multiple pages 
 * @param curr			segment where the allocation starts
 * @param pdata			page data for page where the allocation is
 * @param last_page_index	index of last page in process' contiguous memory space
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
int free_ptr(void* p, uint32_t curr_id) {
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
	int process_index = (firstMalloc == 1) ? last_page_index+1 : first_page_index; //index of page in contig memory according to process
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

void mydeallocate(void* p, char* file, int line, int threadreq) {
	enter_scheduler(&timer_pause_dump);
	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		error_message("Can't free null pointer!", file, line);
		return;
	}
	
	uint32_t curr_id = (threadreq == LIBRARYREQ) ? 0 : ( (tcb*) get_head(ready_q[curr_prio]) )->id;
	
//	printf("free call %s:%d %p\n", file, line, p);
	int d = free_ptr(p, curr_id);
//	printMemory();
	if (d == 1) {
		error_message("Cannot free an already free pointer!", file, line);
	}
	else if (d==2) {
		error_message("This pointer was not malloc'd!", file, line);
	}
	exit_scheduler(&timer_pause_dump);
}
