#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"

static char* myblock;
static int firstMalloc = 1;
struct sigaction* act;
size_t page_size;  //size of page given to each process
int num_pages;
int num_segments;
char* mem_space;

static void handler(int sig, siginfo_t* si, void* unused) {
  printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
}

/**
 * Frees data allocated by memory management system during process teardown.
 */
void free_mem_metadata() {
  mydeallocate(act, __FILE__, __LINE__, LIBRARYREQ);
}

//user blocks are occupied (malloc'd) if the
//left most bit of 8 bit of the metadata is 1,
//so to check if a user data block is occupied:
//calculate bitwise '&' of size and 0x80 (0b 1000 0000)
//if result is 0, then the block is free, 
//and return this so is_occupied = false
//else we will return 0x80 which is true 
int is_occupied(metadata* curr) {
	return  (curr->size) & 0x80;
}

//ignores the leftmost bit in the metadata which represents free/malloc
//to return the number of userdata blocks
unsigned char block_size(metadata* curr) {
	return curr -> size & 0x7f;
}


//prints any error messages followed by memory 
void error_message(char* error, char* file, int line) {
	printf("%s:%d: %s\n", file, line, error);
}

//takes the number of segments within page to be free and writes that into metadata
void write_free_size(metadata* curr, size_t newSize) {
	curr -> size = newSize & 0x7f;
	return;
}

//takes the number of segments within page to be malloc'd
//and makes the leftmost bit of the byte-sized number 1
//to represent malloc'd and writes this into metadata
//if the current free block is being split with this malloc
//also creates a metadata block to adjust the size of free userdata available
void write_occupied_size(metadata* curr, unsigned char curr_seg, size_t new_size) {
	//if the size of the free block is more than the new size,
	//need to split the block into a first block which is malloc'd
	//and second block which is still free but has a smaller size 
	if (new_size < curr_seg) write_free_size(curr+new_size, curr_seg - new_size);
	curr -> size = (new_size & 0x7f) | 0x80;
}


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
	printf("-------------------------------------------------\n");
	printf("                      MEMORY                     \n");
	printf("-------------------------------------------------\n");
	for (int page_index = 0 ; page_index < num_pages; page_index++) {
		pagedata* pdata = (pagedata*)myblock+page_index;
		if (!is_occupied_page(pdata)) {continue;}
		metadata* start = (metadata*) (mem_space+page_index*page_size);
		int seg_index = 0;
		printf("--------------------PAGE--------------------\n");
		while (seg_index < num_segments) {
			metadata* curr = start + seg_index;
			int isOcc = is_occupied(curr);
			int seg_size = block_size(curr);
			printf("page[%d][%d] pid %d: %d %d %p\n", page_index, seg_index, pdata->pid, isOcc, seg_size, start+num_segments+seg_index*seg_size);
			seg_index+=seg_size;
		}
	}
	printf("-------------------------------------------------\n");
	printf("                      MEMORY                     \n");
	printf("-------------------------------------------------\n");
}

void initialize_pages() {
	for (unsigned short i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		write_free_page(pdata, i);
		metadata* mdata = (metadata*) ((char*)mem_space + i*page_size);
		write_free_size(mdata, num_segments);
	}
}


//returns to the user a pointer to the amount of memory requested
void* myallocate(size_t size, char* file, int line, int threadreq){
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		error_message("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}


	uint32_t curr_id = (threadreq == LIBRARYREQ) ? 0 : ( (tcb*) get_head(ready_q[curr_prio]) )->id;	
	if (firstMalloc == 1) { // first time using malloc
		myblock = memalign(sysconf(_SC_PAGE_SIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGE_SIZE);
		num_segments = page_size / ( SEGMENTSIZE + sizeof(metadata) );
		num_pages = MEMSIZE / ( page_size + sizeof(pagedata));
		mem_space = (char*) ((pagedata*) myblock + num_pages);
		initialize_pages();
		firstMalloc = 0;

		act = myallocate(sizeof(struct sigaction), __FILE__, __LINE__, LIBRARYREQ);
		atexit(free_mem_metadata);
		act->sa_sigaction = handler;
		act->sa_flags = SA_SIGINFO;
		sigemptyset(&act->sa_mask);
		sigaddset(&act->sa_mask,SIGALRM); // block scheduling attempts while resolving memory issues
		sigaction(SIGSEGV, act, NULL);
	}

	//find page for process...
	size_t segments_alloc = size / SEGMENTSIZE + 1;
	int free_page_index = -1;
	int proc_has_page = -1;
	for (int page_index = 0 ; page_index < num_pages; page_index++) {
		pagedata* pdata = (pagedata*)myblock+page_index;
		if (!is_occupied_page(pdata)){ 
			if (free_page_index == -1) free_page_index = page_index;
		}
		else if (pdata->pid == curr_id) {
			if (proc_has_page == -1) proc_has_page = page_index;
			metadata* start = (metadata*) (mem_space+page_index*page_size);
			int seg_index = 0;
			while (seg_index < num_segments) {
				metadata* curr = start + seg_index;
				int seg_size = block_size(curr);
				if ( !is_occupied(curr) && segments_alloc <= seg_size){ 
					write_occupied_size(curr, seg_size, segments_alloc);
					void* ret = (void*) (start+num_segments+seg_index*SEGMENTSIZE);
					printf("malloc %p\n", ret);
					return ret; 	
				}
				seg_index+=seg_size;
			}
		}
		
	}
	
	//here, then no malloc, need a new page
	if (free_page_index != -1 && (proc_has_page == -1 || curr_id == 0) ) {
		pagedata* pdata = (pagedata*)myblock+free_page_index;
		write_occupied_page(pdata, curr_id, free_page_index);
		metadata* start = (metadata*) (mem_space+free_page_index*page_size);
		int seg_size = block_size(start); 
		if (segments_alloc <= seg_size) {
			write_occupied_size(start, seg_size, segments_alloc);
			void* ret = (void*) (start+num_segments);
			printf("malloc %p\n", ret);
			return ret;
		}
	}

	error_message("Not enough memory", file, line);
	return NULL;
}
//frees up any memory malloc'd with this pointer for future use
void mydeallocate(void* p, char* file, int line, int threadreq) {
	//if the pointer is NULL, there is nothing we can free
	printf("free %p\n", p);
	if (p == NULL) {
		error_message("Can't free null pointer!", file, line);
		return;
	}
	
	int page_index = ( (unsigned long) p - (unsigned long) mem_space) / page_size; 
	metadata* start = (metadata*) (mem_space+page_index*page_size); 
	metadata* prev;
	int prev_free = 0;
	int prev_size = 0;
	int seg_index = (firstMalloc == 1) ? num_segments : 0;
	while (seg_index < num_segments) {
		metadata* curr = start + seg_index;
		unsigned char seg_size = block_size(curr);
		if (start+num_segments+seg_index*SEGMENTSIZE == p) {
			if (!is_occupied(curr)) {
				error_message("Cannot free an already free pointer!", file, line);
				return; 
			}
			else {
				write_free_size(curr, seg_size);
				if (seg_index+seg_size < num_segments && !is_occupied(start+seg_index+seg_size)){
					write_free_size(curr, seg_size+block_size(start+seg_index+seg_size));
				}
				if (prev_free) {
					write_free_size(prev, block_size(curr)+prev_size);
				}
				if ( !is_occupied(start) && block_size(start) == num_segments) write_free_page( (pagedata*)myblock+page_index, page_index);
				return; 

			}
		}
		prev = curr;
		if ( !is_occupied(prev)) {
			prev_free = 1;
			prev_size = seg_size;
		}
		else {
			prev_free = 0;
		}
		seg_index+=seg_size;
	}

	//if we did not return within the while loop, the pointer was not found and thus was not malloc'd
	error_message("This pointer was not malloc'd!", file, line);
}
