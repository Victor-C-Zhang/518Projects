#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "my_pthread_t.h"
#include "direct_mapping.h"
#include "open_address_ht.h"
#include "memory_finder.h"

static int firstMalloc = 1;
struct sigaction segh;

static void seg_handler(int sig, siginfo_t* si, void* unused) {
  printf("Got fault\n");
  // can only access memory in block
  if ((char*)si->si_addr < mem_space || (char*)si->si_addr >= (mem_space +
  page_size*num_pages)) {
    printf("Illegal address: 0x%lx\n",(long) si->si_addr);
    exit(1);
  }
  // handle protection level queries
  if (prot_request) {
    prot_response = MF_PROTECTED;
    int pagenum = ((char*)si->si_addr - mem_space)/page_size;
    // allow access to prevent infinite loop
    mprotect(mem_space + page_size*pagenum, page_size, PROT_READ | PROT_WRITE);
    return;
  }
  // verify thread owns the requested page
  int pagenum = ((char*)si->si_addr - mem_space)/page_size;
  ht_val actual_loc;
  // probably should be querying
  if (in_scheduler) actual_loc = ht_get(ht_space,0,pagenum);
  else if (initScheduler) actual_loc = ht_get(ht_space,2,pagenum);
  else actual_loc = ht_get(ht_space, ((tcb*)get_head(ready_q[curr_prio]))->id,pagenum);

  if (actual_loc == HT_NULL_VAL) {
    printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
    exit(1);
  }
  // find requested page
  mprotect(mem_space + page_size*pagenum, page_size, PROT_READ | PROT_WRITE);
  if (actual_loc != pagenum) {
    swap(pagenum,actual_loc);
  }
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
	if (threadreq == THREADREQ) pause_timer(&timer_pause_dump);
	
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		error_message("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}
//  if (initScheduler && threadreq != LIBRARYREQ) printf("Didn't init scheduler\n");
	my_pthread_t curr_id;
	switch (threadreq) {
	  case LIBRARYREQ: {
	    curr_id = 0;
	    break;
	  }
	  case THREADREQ:
	  case STACKREQ: {
	    curr_id = (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id;
	    break;
	  }
	}

	if (firstMalloc == 1) { // first time using malloc
	  // init runtime constants,
    // calculate size of necessary metadata/auxiliary structures
		myblock = memalign(sysconf( _SC_PAGESIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGESIZE);
		num_segments = page_size / SEGMENTSIZE;
    stack_page_size = (STACKSIZE % page_size) ?
          STACKSIZE/page_size + 1 : STACKSIZE/page_size;
		num_pages = (MEMSIZE - stack_page_size * page_size - 1) / (page_size +
		      sizeof(pagedata) + sizeof(ht_entry));
		my_pthread_t pt_space = num_pages*sizeof(pagedata);
		pt_space = (pt_space % page_size) ? pt_space/page_size + 1 : pt_space/page_size;

		// leave space at the beginning for
		// - (inverted) PT
		// - scheduler stack
		// - scheduler context
		// - scheduler tcb
		// - main tcb
		sched_stack_ptr_ = myblock + pt_space * page_size;
		scheduler_context = (ucontext_t*)(myblock + (pt_space + stack_page_size) *
		      page_size);
		scheduler_tcb = (tcb*)((char*)scheduler_context + sizeof(ucontext_t));
		main_tcb = scheduler_tcb + 1;
		mem_space = myblock + (pt_space + stack_page_size + 1) * page_size;

    // init tcbs for main, scheduler
    scheduler_tcb->first_page_index = UINT16_MAX;
    scheduler_tcb->last_page_index = -1;
    main_tcb->first_page_index = UINT16_MAX;
    main_tcb->last_page_index = -1;

    ht_space = (ht_entry*) (mem_space + page_size*num_pages);
    createTable(ht_space);
		initialize_pages();
		prot_request = 0;
		prot_response = MF_UNPROTECTED;

		memset(&segh, 0, sizeof(struct sigaction));
		sigemptyset(&segh.sa_mask);
		segh.sa_sigaction = seg_handler;
		segh.sa_flags = SA_SIGINFO;
		sigaddset(&segh.sa_mask,SIGALRM); // block scheduling attempts while resolving memory issues
		sigaction(SIGSEGV, &segh, NULL);
		firstMalloc = 0;
		// workaround to prevent swapping scheduling pages into stack space
    myallocate(stack_page_size*page_size - 1, __FILE__, __LINE__, LIBRARYREQ);
	}
	
	void* p = segment_allocate(size, curr_id);
//	printf("malloc %d call %s:%d %p\n", size / SEGMENTSIZE+1, file, line, p);
//	printMemory();
	if (p == NULL) {
		error_message("Not enough memory", file, line);
	}
	if (threadreq == THREADREQ) resume_timer(&timer_pause_dump);
	return p;
}

void mydeallocate(void* p, char* file, int line, int threadreq) {
	if (threadreq == THREADREQ) pause_timer(&timer_pause_dump);
	//if the pointer is NULL, there is nothing we can free
	if (p == NULL) {
		error_message("Can't free null pointer!", file, line);
    exit_scheduler_context(&timer_pause_dump);
		return;
	}
	if (firstMalloc) {
		error_message("This pointer was not malloc'd!", file, line);
    exit_scheduler_context(&timer_pause_dump);
		return;
	}
	my_pthread_t curr_id;
  switch (threadreq) {
    case LIBRARYREQ: {
      curr_id = 0;
      break;
    }
    case THREADREQ:
    case STACKREQ: {
      curr_id = (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id;
      break;
    }
  }
	
//	printf("free call %s:%d %p\n", file, line, p);
	int d = free_ptr(p, curr_id);
//	printMemory();
	if (d == 1) {
		error_message("Cannot free an already free pointer!", file, line);
	}
	else if (d==2) {
		error_message("This pointer was not malloc'd!", file, line);
	}
	if (threadreq == THREADREQ) resume_timer(&timer_pause_dump);
}
