#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"

static char* myblock;
static int firstMalloc = 1;
struct sigaction* act;
size_t page_size;  //size of page given to each process
size_t segment_size; //size of segments within page
int num_pages;
char* mem_space;

static void handler(int sig, siginfo_t* si, void* unused) {
  printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
}

/**
 * Frees data allocated by memory management system during process teardown.
 */
void free_mem_metadata() {
  free(act);
}

//user blocks are occupied (malloc'd) if the
//left most bit of 16 bit of the metadata block is 1,
//so to check if a user data block is occupied:
//calculate bitwise '&' of size and 0x8000 (0b 1000 0000 0000 0000)
//if result is 0, then the block is free, 
//and return this so isOccupied = false
//else we will return 0x8000 which is true 
int isOccupied(metadata* curr) {
	return  (curr->size) & 0x80;
}

//ignores the leftmost bit in the metadata which represents free/malloc
//to return the size of userdata block
unsigned char blockSize(metadata* curr) {
	return curr -> size & 0x7f;
}

//for debugging and error reporting purposes
//prints memory as index of memory
//followed by 0 for free or any other integer for occupied
//followed by the size of user data
void printMemory() {
	printf("-----------------------MEMORY-----------------------\n");
	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		metadata* mdata = (metadata*) ((char*)mem_space + i*segment_size);
		printf("--------------------PAGE--------------------\n");
		for (int j = 0; j < NUMSEGMENTS; j++) {
			char currSize = blockSize(mdata);
			int isOcc = isOccupied(mdata);
			printf("page[%d][%d] pid %d: %d  %hu\n", i, j, pdata->pid, isOcc, currSize);
			mdata+=currSize;	
		}
		printf("--------------------PAGE--------------------\n");
	}
	printf("-----------------------MEMORY-----------------------\n");
}

//prints any error messages followed by memory 
void errorMessage(char* error, char* file, int line) {
	printf("%s:%d: %s\n", file, line, error);
	printMemory();
}

//takes the number of segments within page to be free and writes that into metadata
void writeFreeSize(metadata* curr, size_t newSize) {
	curr -> size = newSize & 0x7f;
	return;
}

//takes the number of segments within page to be malloc'd
//and makes the leftmost bit of the 1 byte number 1
//to represent malloc'd and writes this into metadata
//if the current free block is being split with this malloc
//also creates a metadata block to adjust the size of free userdata available
void writeOccupiedSize(metadata* curr, unsigned char currSize, size_t newSize) {
	//if the size of the free block is more than the new size,
	//need to split the block into a first block which is malloc'd
	//and second block which is still free but has a smaller size 
	if (newSize < currSize) {
		writeFreeSize(curr+newSize, (currSize - newSize - sizeof(metadata)));
	}
	
	curr -> size = (newSize & 0x7f) | 0x80;
}

void initialize_pages() {
	for (unsigned short i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		pdata->pid = -1;
		pdata->p_ind = i;
		pdata->swapped_to = -1;
		metadata* mdata = (metadata*) ((char*)mem_space + i*segment_size);
		writeFreeSize(mdata, NUMSEGMENTS);
	}
}


//returns to the user a pointer to the amount of memory requested
void* myallocate(size_t size, char* file, int line, int threadreq){
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		errorMessage("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}
	
	uint32_t curr_id = (threadreq == LIBRARYREQ) ? 0 : ( (tcb*) get_head(ready_q[curr_prio]) )->id;
	if (firstMalloc == 1) { // first time using malloc
		myblock = memalign(sysconf(_SC_PAGE_SIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGE_SIZE);
		segment_size = page_size / NUMSEGMENTS - sizeof(metadata); 
		num_pages = MEMSIZE / (page_size + sizeof(pagedata) ); 
		mem_space = myblock + num_pages * sizeof(pagedata);
		initialize_pages();

		firstMalloc = 0;

		act = myallocate(sizeof(struct sigaction), __FILE__, __LINE__, LIBRARYREQ);
		atexit(free_mem_metadata);
		act->sa_sigaction = handler;
		act->sa_flags = SA_SIGINFO;
		sigemptyset(&act->sa_mask);
		sigaddset(&act->sa_mask,SIGALRM); // block scheduling attempts while
		// resolving memory issues
		sigaction(SIGSEGV, act, NULL);	
	}

	//find page for process...
	size_t segments_alloc = size / segment_size + 1; 
	int free_page = -1;
	int page_exist = -1; 
	int index;
	//if exists, find first fit free space
	for (index = 0; index < num_pages; index++) {
		pagedata* pdata = (pagedata*)myblock + index;	
		if (pdata->pid == -1 && free_page == -1) {
			free_page = index;
		}

		if (pdata->pid == curr_id) {
			if (page_exist == -1) {
				page_exist = index; //for swapping later... 
			}
			
			//traverse memory to find the first free block that can fit the size requested
			metadata* start = (metadata*) ((char*)mem_space + index*segment_size);
			int seg_index = 0;
			while (seg_index < NUMSEGMENTS) {
				metadata* curr = start + seg_index;
				unsigned char currSize = blockSize(curr);
				if (!isOccupied(curr)) {
					// if block is not occupied, it is free
					// we can malloc this block only if
					// 1. the size requested equals the block size, so we have space to assign metadata and user data
					// 2. the size requested is less than the block size
					if (segments_alloc == currSize || (segments_alloc < currSize) ) { 
						writeOccupiedSize(curr, currSize, segments_alloc);
						//segment memory space = start + NUMSEGMENTS
						//free segment = segment memory space + seg_index * segment_size; 
						return (void*)( start+NUMSEGMENTS + seg_index * segment_size);
					}
				}
				seg_index+=currSize;
			}
			// otherwise we don't have enough space, and need to keep searching
		}		
	}
	
	//if index is more than or equal to num_pages, we did not find page for current process
	//so allocate page, then return pointer within page chunk
	if (index >= num_pages) {
		//if free_page == -1, there are not free pages available
		if (free_page == -1) {
			errorMessage("Not enough memory", file, line);
			return NULL;
		}
		pagedata* pdata = (pagedata*)myblock+free_page;
		pdata->pid = curr_id;
		pdata->p_ind = free_page; //change when we do phase b 
		metadata* curr = (metadata*) ((char*)mem_space + free_page*segment_size);
		unsigned char currSize = blockSize(curr);
		if (segments_alloc == currSize || (segments_alloc < currSize) ) { 
			writeOccupiedSize(curr, currSize, segments_alloc);
			//segment memory space = start + NUMSEGMENTS
			//free segment = segment memory space + seg_index * segment_size; 
			return (void*)( curr+NUMSEGMENTS);
		}
		return NULL; 
	}
}

//frees up any memory malloc'd with this pointer for future use
void mydeallocate(void* p, char* file, int line, int threadreq) {
	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		errorMessage("Can't free null pointer!", file, line);
		return;
	}
	
	int seg_index = 0;
	//if nothing has been malloc'd yet, cannot free!
	if (firstMalloc == 1) { // first time using malloc
		seg_index = NUMSEGMENTS;
	}
	
	int index = ( (unsigned long) p - (unsigned long) mem_space) / segment_size;
	metadata* start = (metadata*) ((char*)mem_space + index*segment_size);
	metadata* prev;
	int prevFree = 0;
	unsigned char prevSize = 0;
	//find the pointer to be free and keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (seg_index < NUMSEGMENTS) { 
		metadata* curr = start + seg_index;
		unsigned char currSize = blockSize(curr);
		//if we find the pointer to be free'd
		if ( (start+NUMSEGMENTS + seg_index * segment_size) == p) {
			//if it is already free, cannot free it -> error
			if ( !isOccupied(curr) ) {
				errorMessage("Cannot free an already free pointer!", file, line);
				return;
			}
			else {
				//free the current pointer
				writeFreeSize(curr, currSize);

				//if the next block is within segment and is free, combine if with my current pointer as a free block
				if (seg_index+currSize < NUMSEGMENTS){
					metadata* next = start + seg_index + currSize;
					if (!isOccupied(next)){
						writeFreeSize(curr, currSize+blockSize(next));
					}
				}

				//if previous block was free, combine my current pointer with previous block
				if (prevFree) {
					writeFreeSize(prev, prevSize+blockSize(curr));
				}
				return;
			}		
		}
		prev = curr;
		if (!isOccupied(curr)) {
			prevFree = 1;
			prevSize = currSize;
		}
		else { 
			prevFree = 0;
		}
		seg_index = seg_index+currSize;
	}

	if ( !isOccupied(start) && blockSize(start) == NUMSEGMENTS) {
		pagedata* pdata = (pagedata*)myblock + index;
		pdata->pid = -1;
		pdata->p_ind = -1;
	}

	//if we did not return within the while loop, the pointer was not found and thus was not malloc'd
	errorMessage("This pointer was not malloc'd!", file, line);
}
