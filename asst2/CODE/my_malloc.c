#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "my_pthread_t.h"
#include "direct_mapping.h"
#include "open_address_ht.h"
#include "memory_finder.h"

static int firstMalloc = 1;
struct sigaction segh;

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

//TODO: update num_pages in phase c
void* myallocate(size_t size, char* file, int line, int threadreq){
	enter_scheduler(&timer_pause_dump);
	
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		error_message("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}
//  if (initScheduler && threadreq != LIBRARYREQ) printf("Didn't init scheduler\n");
	my_pthread_t curr_id = (threadreq == LIBRARYREQ) ? 0 : (
        (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id
  );

	if (firstMalloc == 1) { // first time using malloc
		myblock = memalign(sysconf( _SC_PAGESIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGESIZE);
		num_segments = page_size / SEGMENTSIZE;
		num_pages = (MEMSIZE - 1) / ( page_size + sizeof(pagedata) + sizeof
		      (ht_entry) );
		my_pthread_t pt_space = num_pages*sizeof(pagedata);
		pt_space = (pt_space % page_size) ? pt_space/page_size + 1 : pt_space/page_size;

		// init tcbs for main, scheduler
		scheduler_tcb = (tcb*)(myblock + pt_space * page_size);
		scheduler_tcb->first_page_index = UINT16_MAX;
		scheduler_tcb->last_page_index = -1;
		main_tcb = scheduler_tcb + 1;
		main_tcb->first_page_index = UINT16_MAX;
		main_tcb->last_page_index = -1;

		mem_space = myblock + (pt_space + 1)*page_size;
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

void mydeallocate(void* p, char* file, int line, int threadreq) {
	enter_scheduler(&timer_pause_dump);
	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		error_message("Can't free null pointer!", file, line);
		exit_scheduler(&timer_pause_dump);
		return;
	}
	if (firstMalloc) {
		error_message("This pointer was not malloc'd!", file, line);
		exit_scheduler(&timer_pause_dump);
		return;
	}
  my_pthread_t curr_id = (threadreq == LIBRARYREQ) ? 0 : (
        (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id
  );
	
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
