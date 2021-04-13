#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"
#include "my_scheduler.h" 

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
  // oops, we probably can't do this, cant free myblock bc that needs to be done by user
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
unsigned short blockSize(metadata* curr) {
	return curr -> size & 0x7f;
}

//for debugging and error reporting purposes
//prints memory as index of memory
//followed by 0 for free or any other integer for occupied
//followed by the size of user data
void printMemory() {
	int index = sizeof(metadata);
	metadata* curr = (metadata*) (myblock+sizeof(metadata));
	printf("---------------------------------------\n");
	while (index < MEMSIZE) {
		unsigned short currSize = blockSize(curr);
		int isOcc = isOccupied(curr);
		printf("myblock[%d]: %d %hu\n", index, isOcc, currSize);
		index = index+sizeof(metadata)+currSize;
		curr =   (metadata*) ( (char*)myblock + index);
	}
	printf("---------------------------------------\n");
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
void writeOccupiedSize(metadata* curr, unsigned short currSize, size_t newSize) {
	//if the size of the free block is more than the new size,
	//need to split the block into a first block which is malloc'd
	//and second block which is still free but has a smaller size 
	if (newSize < currSize) {
		writeFreeSize(curr+1, (currSize - newSize - sizeof(metadata)));
	}
	
	curr -> size = (newSize & 0x7f) | 0x80;
}

void initialize_pages() {
	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		pdata->pid = -1;
		pdata->p_ind = -1;
		pdata->swapped_to = -1;
		metadata* mdata = (metadata*) ((char*)mem_space + i*SEGMENTSIZE);
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
	
	if (firstMalloc == 1) { // first time using malloc
		myblock = memalign(sysconf(_SC_PAGE_SIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGE_SIZE);
		segment_size = page_size / NUMSEGMENTS; 
		num_pages = MEMSIZE / (page_size + sizeof(pagedata) ); 
		mem_space = myblock + num_pages * sizeof(pagedata);
		initialize_pages();
		pagedata* pdata = (pagedata*)myblock;	
		pdata->pid = (tcb*) get_head(ready_q[curr_prio])->id;
		pdata->p_ind = 0;

		firstMalloc = 0;

		act = malloc(sizeof(struct sigaction));
		atexit(free_mem_metadata);
		act->sa_sigaction = handler;
		act->sa_flags = SA_SIGINFO;
		sigemptyset(&act->sa_mask);
		sigaddset(&act->sa_mask,SIGALRM); // block scheduling attempts while
		// resolving memory issues
		sigaction(SIGSEGV, act, NULL);	
	}

	//find page for process...
		//if exists, find first fit free space
		//if doesn't exist, find free page, allocate page, then return pointer within page chunk


	unsigned short currSize = 0;
	metadata* curr =  (first+1);
	int index = sizeof(metadata);
	//traverse memory to find the first free block that can fit the size requested
	while (index < MEMSIZE) {
		currSize = blockSize(curr);
		if (!isOccupied(curr)) {
			// if block is not occupied, it is free
			// we can malloc this block only if
			// 1. the size requested equals the block size, so we have space to assign metadata and user data
			// 2. the size requested is less than the block size, but in this case we will be splitting the block
				//so we would actually need space for 1 more metadata and at least 1 byte of userdata and of course size requested, 
				//so the (size requested)+sizeof(metadata) must be LESS THAN and NOT equal to size available!
			if (size == currSize || (size + sizeof(metadata) < currSize) ) { 
				break;
			}
			// otherwise we don't have enough space, and need to keep searching
		}
		//adding sizeof(metadata) to the current index will take us to the start of this block's user data
		//then adding currSize (or size of the users data) will take us to the start of the next block
		//which is also that block's metadata
		index = index+sizeof(metadata)+currSize;
		curr =   (metadata*) ( (char*)myblock + index);
	}

	//if index is more than or equal to MEMSIZE, we did not find an empty block	
	if (index >= MEMSIZE) {
		errorMessage("Not enough memory", file, line);
		return NULL; 
	}
	
	writeOccupiedSize(curr, currSize, size);
	return (void*)(++curr);
}

//frees up any memory malloc'd with this pointer for future use
void mydeallocate(void* p, char* file, int line, int threadreq) {

	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		errorMessage("Can't free null pointer!", file, line);
		return;
	}
	
	int currIndex = sizeof(metadata);
	//if nothing has been malloc'd yet, cannot free!
	metadata* first=(metadata*)myblock;
	if (first->size != 12144) {
		currIndex = MEMSIZE;
	}

	metadata* prev;
	int prevFree = 0;
	unsigned short prevSize = 0;
	metadata* curr = (metadata*) (first+1);
	//find the pointer to be free and keep track of the previous block incase it is free so we can combine them and avoid memory fragmentation
	while (currIndex < MEMSIZE) {
		unsigned short currSize = blockSize(curr);
		//if we find the pointer to be free'd
		if ( (char*) (curr+1) == (char*) p) {
			//if it is already free, cannot free it -> error
			if ( !isOccupied(curr) ) {
				errorMessage("Cannot free an already free pointer!", file, line);
				return;
			}
			else {
				//free the current pointer
				writeFreeSize(curr, currSize);

				//if the next block is within myblock and is free, combine if with my current pointer as a free block
				int nextIndex = currIndex+sizeof(metadata)+currSize;
				if (nextIndex < MEMSIZE){
					metadata* next = (metadata*) ((char*)myblock+nextIndex);
					if (!isOccupied(next)){
						writeFreeSize(curr, currSize+sizeof(metadata)+blockSize(next));
					}
				}

				//if previous block was free, combine my current pointer with previous block
				if (prevFree) {
					writeFreeSize(prev, prevSize+sizeof(metadata)+blockSize(curr));
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
		currIndex = currIndex+sizeof(metadata)+currSize;
		curr =   (metadata*) ( (char*)myblock + currIndex);
	}
	
	//if we did not return within the while loop, the pointer was not found and thus was not malloc'd
	errorMessage("This pointer was not malloc'd!", file, line);
}
