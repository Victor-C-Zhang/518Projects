#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "direct_mapping.h"

static char* myblock;
static int firstMalloc = 1;
struct sigaction segh;
size_t page_size;  //size of page given to each process
int num_pages; //number of pages available for user
int num_segments; //number of segments within a page available for allocation 
char* mem_space; //user space, after page table

static void handler(int sig, siginfo_t* si, void* unused) {
	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
	exit(0);
}

//prints any error messages followed by memory 
void error_message(char* error, char* file, int line) {
	printf("%s:%d: %s\n", file, line, error);
	exit(0);
}

//takes the number of segments within page to be malloc'd
//and makes the leftmost bit of the byte-sized number 1
//to represent malloc'd and writes this into metadata
//if the current free block is being split with this malloc
//also creates a metadata block to adjust the size of free userdata available
//void write_occupied_size(metadata* curr, unsigned char curr_seg, size_t newSize) {
//	//if the size of the free block is more than the new size,
//	//need to split the block into a first block which is malloc'd
//	//and second block which is still free but has a smaller size
//	if (newSize < curr_seg)
//		dm_write_metadata(curr + newSize, (curr_seg - newSize));
//	curr -> size = (newSize & 0x7f) | 0x80;
//}

int is_occupied_page(pagedata* curr) {
	return  (curr->p_ind) & 0x8000;
}

void write_free_page(pagedata* curr, unsigned short index) {
  curr->p_ind = (index & 0x7fff);
}

void write_occupied_page(pagedata* curr, uint32_t pid, unsigned short index) {
  curr->pid = pid;
  curr->p_ind = (index & 0x7fff) | 0x8000;
}
//for debugging and error reporting purposes
//prints memory as index of memory
//followed by 0 for free or any other integer for occupied
//followed by the size of user data
void printMemory() {
	printf("-----------------------MEMORY-----------------------\n");
	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		if (!is_occupied_page(pdata)) {continue;}
		metadata* start = (metadata*) ((char*)mem_space + i*page_size);
		printf("--------------------PAGE--------------------\n");
		int j = 0;
		while (j < num_segments) {
			metadata* mdata = start + j;
			char curr_seg = dm_block_size(mdata);
			int isOcc = dm_block_occupied(mdata);
			printf("page[%d][%d] pid %d %p: %d  %hu %p\n", i, j, pdata->pid, mdata, isOcc, curr_seg, (start+num_segments) + j*SEGMENTSIZE);
			j+=curr_seg;	
		}
	}
	printf("-----------------------MEMORY-----------------------\n");
}

void initialize_pages() {
	for (unsigned short i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		write_free_page(pdata, i);
		metadata* mdata = (metadata*) ((char*)mem_space + i*page_size);
		dm_write_metadata(mdata, num_segments, 0, 1);
	}
}

void* find_free_page(size_t size, uint32_t curr_id) {
	//find page for process...
	size_t segments_alloc = size / SEGMENTSIZE + 1;
	int free_page = -1;
	int page_exist = -1;
	int index = (segments_alloc > num_segments) ? num_pages : 0; // remove this
	// check to allow longer allocations

	//if exists, find first fit free space
	for (; index < num_pages; index++) {
		pagedata* pdata = (pagedata*)myblock + index;	
		if ( !is_occupied_page(pdata) ) { //free page
			if (free_page == -1) {
				free_page = index;
			}
		}
		else if (pdata->pid == curr_id) { //occupied page
			if (page_exist == -1) {
				page_exist = index; //for swapping later...
			}
			//traverse memory to find the first free block that can fit the size requested
			metadata* start = (metadata*) (mem_space + index*page_size);
			int seg_index = 0;
			while ( seg_index < num_segments) {
				metadata* curr = start + seg_index*SEGMENTSIZE;
				unsigned char curr_seg = dm_block_size(curr);
				if (!dm_block_occupied(curr) && (segments_alloc <= curr_seg ) ) {
					dm_allocate_block(curr, segments_alloc);
					//segment memory space = start + num_segments
					//free segment = segment memory space + seg_index * SEGMENTSIZE;
					return (void*) (curr + 1);
				}
				seg_index+=curr_seg;
			}
		}
		//else not enough space, find any other pages allocated to pid
	}

	//we did not find page for current process
	//so allocate page, then return pointer within page chunk
	if ( (page_exist == -1 || curr_id == 0) && free_page != -1 ) {
		write_occupied_page((pagedata*)myblock+free_page, curr_id, free_page);
		metadata* curr = (metadata*) (mem_space + free_page*page_size);
		unsigned char curr_seg = dm_block_size(curr);
		if (segments_alloc <= curr_seg) { 
			dm_allocate_block(curr, segments_alloc);
			return (void*) (curr + 1);
		}
	}

	return NULL;
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
		num_pages = MEMSIZE / ( page_size + sizeof(pagedata));
		uint32_t pt_space = num_pages*sizeof(pagedata);
		pt_space = (pt_space % page_size) ? pt_space/page_size + 1 : pt_space/page_size;
		mem_space = myblock + pt_space*page_size;
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

int free_ptr(void* p) {	
	int page_index = ( (char*)p - 1 - mem_space) /
				page_size ;
	metadata* start = (metadata*) (mem_space + page_index*page_size);
	metadata* prev;
	int prevFree = 0;
	unsigned char prevSize = 0;
	//if nothing has been malloc'd yet, cannot free!
	int seg_index = (firstMalloc == 1) ? num_segments: 0;
	//find the pointer to be free and keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (seg_index < num_segments) { 
		metadata* curr = start + seg_index*SEGMENTSIZE;
		unsigned char curr_seg = dm_block_size(curr);
		//if we find the pointer to be free'd
		if ( curr + 1 == p) {
			//if it is already free, cannot free it -> error
			if ( !dm_block_occupied(curr) ) {
				return 1;
			}
			else {
				// free current pointer
				dm_write_metadata(curr, curr_seg, 0, dm_is_last_segment(curr));
				//if the next block is within segment and is free, combine if with my current pointer as a free block
				if (seg_index+curr_seg < num_segments){
					metadata* next = curr + curr_seg*SEGMENTSIZE;
					if (!dm_block_occupied(next)){
						dm_write_metadata(curr, curr_seg + dm_block_size(next), 0,
												dm_is_last_segment(next));
					}
				}
				//if previous block was free, combine my current pointer with previous block
				if (prevFree) dm_write_metadata(prev, prevSize + dm_block_size(curr),
																		0, dm_is_last_segment(curr));
				if (!dm_block_occupied(start) && dm_block_size(start) == num_segments) {
					write_free_page( (pagedata*)myblock + page_index, page_index );
				}
			}		
			exit_scheduler(&timer_pause_dump);
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
