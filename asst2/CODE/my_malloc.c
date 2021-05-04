#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "my_malloc.h"
#include "global_vals.h"
#include "my_pthread_t.h"
#include "direct_mapping.h"
#include "open_address_ht.h"
#include "memory_finder.h"

static int firstMalloc = 1;
struct sigaction segh;

static void handler(int sig, siginfo_t* si, void* unused) {
//	printf("Got fault at address: 0x%lx\n",(long) si->si_addr);
  // can only access memory in block
  if ((char*)si->si_addr < mem_space || (char*)si->si_addr >= (mem_space +
                                                               page_size*num_pages)) {
    printf("Illegal address: 0x%lx\n",(long) si->si_addr);
    exit(1);
  }
  // verify thread owns the requested page
  int pagenum = ((char*)si->si_addr - mem_space)/page_size;
  my_pthread_t curr_id = mm_curr_id;
  if (curr_id == -1) {
    fprintf(stderr, "my_malloc line 29: Curr_id is -1\n");
  }
  ht_val actual_loc = ht_get(ht_space, curr_id, pagenum);
  if (actual_loc == HT_NULL_VAL) {
    printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
    printf("Could not get page %d of id %d\n", pagenum, curr_id);
    exit(1);
  }
  // find and swap requested page
  mprotect(mem_space + page_size*pagenum, page_size, PROT_READ | PROT_WRITE);
  if (actual_loc != pagenum) {
    if (actual_loc < resident_pages)
      mprotect(mem_space + page_size*actual_loc, page_size, PROT_READ | PROT_WRITE);
    swap(pagenum, actual_loc);
    if (actual_loc < resident_pages)
      mprotect(mem_space + page_size*actual_loc, page_size, PROT_NONE);
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
  mprotect(mem_space, page_size*resident_pages, PROT_READ | PROT_WRITE);
	printf("<----------------------MEMORY---------------------->\n");
	int ovf_len = 0;
	int i = 0;
  int j = 0;
  while (i < resident_pages){
//	for (int i = 0; i < num_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
//		int j = ovf_len;
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
      if (pg_is_overflow(pdata) && isLast) --curr_seg;
      printf("page[%d][%d]\tpid %d\tp_ind %d\t ovf %d\t ovflen %d: %p %d %d %hu\n",
					i, j, pdata->pid, pg_index(pdata), pg_is_overflow(pdata), pdata->length, mdata, isOcc, isLast, curr_seg);
			if (isLast) {
        printf("--------------------PAGE--------------------\n");
        if (pg_is_overflow(pdata)) {
				  i += pdata->length;
				  j += curr_seg;
				  if (j >= num_segments) {
				    ++i;
				    j %= num_segments;
				  }
				} else {
          ++i;
          j = 0;
        }
        break;
			} else {
			  j += curr_seg;
			}
		}
	}
	printf("</---------------------MEMORY---------------------->\n");
  mprotect(mem_space, page_size*resident_pages, PROT_NONE);

}

void close_swapfile() {
  close(swapfile);
}

void initialize_pages() {
	for (int i = 0; i < resident_pages; i++) {
		pagedata* pdata = (pagedata*)myblock + i;	
		pg_write_pagedata(pdata, -1, NOT_OCC, NOT_OVF, -1, 1);
		metadata* mdata = (metadata*) ((uint8_t*)mem_space + i*page_size);
		dm_write_metadata(mdata, num_segments, 0, 1);
	}
  for (int i = resident_pages; i < num_pages; ++i) {
    pagedata* pdata = (pagedata*)myblock + i;
    pg_write_pagedata(pdata, -1, NOT_OCC, NOT_OVF, -1, 1);
    metadata writ;
    dm_write_metadata(&writ, num_segments, 0, 1);
    lseek(swapfile, (i - resident_pages)*page_size, SEEK_SET);
    write(swapfile, &writ, 1);
  }
}

my_pthread_t enter_mem_manager(my_pthread_t curr_id) {
  mm_in_memory_manager = 1;
  my_pthread_t retval = mm_curr_id;
  mm_curr_id = curr_id;
  mprotect(mem_space + stack_page_size*page_size, (resident_pages -
  stack_page_size)*page_size, PROT_NONE);
  return retval;
}

void exit_mem_manager(my_pthread_t prev_id) {
  mm_in_memory_manager = 0;
  mprotect(mem_space + stack_page_size*page_size, (resident_pages -
  stack_page_size)*page_size, PROT_NONE);
  mprotect(mem_space, page_size*stack_page_size, PROT_READ | PROT_WRITE);
  mm_curr_id = prev_id;
}

void* myallocate(size_t size, char* file, int line, int threadreq){
	enter_scheduler(&timer_pause_dump);
	my_pthread_t temp_id = enter_mem_manager(0);
	//if the user asks for 0 bytes, call error and return NULL
	if (size <= 0){
		error_message("Cannot malloc 0 or negative bytes", file, line);
		return NULL;
	}
//  if (initScheduler && threadreq != LIBRARYREQ) printf("Didn't init scheduler\n");
  my_pthread_t curr_id;
	my_pthread_t prev_id;
  switch (threadreq) {
    case LIBRARYREQ: {
      curr_id = 0;
      break;
    }
    case THREADREQ: {
      curr_id = (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id;
      break;
    }
    case STACKREQ: {
      curr_id = stack_creat_id;
      break;
    }
    case STACKDESREQ: {
      curr_id = prev_done_id;
      break;
    }
  }

	if (firstMalloc == 1) { // first time using malloc
    atexit(close_swapfile);
	  // init runtime constants
	  // calculate size of necessary metadata/auxiliary structures
		myblock = memalign(sysconf( _SC_PAGESIZE), MEMSIZE);
		page_size = sysconf( _SC_PAGESIZE);
		num_segments = page_size / SEGMENTSIZE;
    stack_page_size = ((STACKSIZE + 1) % page_size) ?
                      (STACKSIZE+1)/page_size + 1 : (STACKSIZE+1)/page_size;
    pages_for_contexts = 200;
    max_thread_id = (int)(pages_for_contexts*page_size)/(int)sizeof(ucontext_t) - 1;
    num_pages = (VIRTSIZE - page_size * (stack_page_size + 1 + pages_for_contexts))
          / (page_size + sizeof(pagedata) + sizeof(ht_entry));
    // leave space for
    // - (inverted) PT
    // - scheduler stack
    // - scheduler TCB
    // - main TCB
    // - context array
		int pt_space = num_pages*sizeof(pagedata);
		pt_space = (pt_space % page_size) ? pt_space/page_size + 1 : pt_space/page_size;

    sched_stack_ptr_ = myblock + pt_space * page_size;
    // init tcbs for main, scheduler
    scheduler_tcb = (tcb*)(myblock + (pt_space + stack_page_size) * page_size);
    scheduler_tcb->has_allocation = 0;
    main_tcb = scheduler_tcb + 1;
    main_tcb->has_allocation = 0;

    mm_contextarr = (ucontext_t*) (myblock + (pt_space + stack_page_size + 1) * page_size);
    scheduler_context = mm_contextarr;
    main_tcb->context = &mm_contextarr[2];
//    getcontext(&mm_contextarr[2]);
    mem_space = myblock + (pt_space + stack_page_size + 1 + pages_for_contexts) * page_size;
    resident_pages = ((myblock + MEMSIZE) - mem_space)/page_size -
          (VIRTSIZE/page_size)*sizeof(ht_entry)/page_size;
		ht_space = (ht_entry*) (mem_space + page_size*resident_pages);
		createTable(ht_space);
		initialize_pages();

    swapfile = open("./SWAPFILE", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (swapfile == -1) {
      fprintf(stderr, "Could not create swapfile. Aborting\n");
      exit(1);
    }
    int alloc_res = posix_fallocate(swapfile, 0, 16*(1<<20));
    if (alloc_res != 0) {
      fprintf(stderr, "Problem allocating swapfile size.\n");
      strerror(alloc_res);
      exit(1);
    }
    // zero the swapfile before we use it
    char zeroes[page_size];
    memset(zeroes, 0, page_size);
    for (int i = resident_pages; i < num_pages; ++i) {
      lseek(swapfile, (i - resident_pages)*page_size, SEEK_SET);
      int to_write = page_size;
      int num_write = 0;
      while (to_write > 0) {
        num_write = write(swapfile, zeroes + num_write, to_write);
        if (num_write == -1) {
          strerror(errno);
          exit(1);
        }
      to_write -= num_write;
      }
    }

		memset(&segh, 0, sizeof(struct sigaction));
		sigemptyset(&segh.sa_mask);
		segh.sa_sigaction = handler;
		segh.sa_flags = SA_SIGINFO;
		sigaddset(&segh.sa_mask,SIGALRM); // block scheduling attempts while resolving memory issues
		sigaction(SIGSEGV, &segh, NULL);

		// protect all pages
    mprotect(mem_space, page_size*resident_pages, PROT_NONE);

		firstMalloc = 0;
	}
  int has_allocations;
  if (curr_id == 0) { //scheduler
    has_allocations = scheduler_tcb->has_allocation;
    scheduler_tcb->has_allocation = 1;
  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
    has_allocations = main_tcb->has_allocation;
    main_tcb->has_allocation = 1;
  } else {
    has_allocations = ((tcb*)get(all_threads, curr_id))->has_allocation;
    ((tcb*)get(all_threads, curr_id))->has_allocation = 1;
  }
  prev_id = enter_mem_manager(curr_id);
// printf("id: %d malloc call %s:%d\n", curr_id, file, line);
  void* p = segment_allocate(size, curr_id, has_allocations);
//	printf("malloc %d call %s:%d %p\n", size / SEGMENTSIZE+1, file, line, p);
//	printMemory();
	if (p == NULL) {
		error_message("Not enough memory", file, line);
	}
	exit_mem_manager(prev_id);
  exit_mem_manager(temp_id);
	exit_scheduler(&timer_pause_dump);
	return p;
}

void mydeallocate(void* p, char* file, int line, int threadreq) {
	enter_scheduler(&timer_pause_dump);
	my_pthread_t temp_id = enter_mem_manager(0);
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
  my_pthread_t curr_id;
	my_pthread_t prev_id;
  switch (threadreq) {
    case LIBRARYREQ: {
      curr_id = 0;
      break;
    }
    case THREADREQ: {
      curr_id = (initScheduler) ? 2 : ((tcb*) get_head(ready_q[curr_prio]))->id;
      break;
    }
    case STACKREQ: {
      curr_id = stack_creat_id;
      break;
    }
    case STACKDESREQ: {
      curr_id = prev_done_id;
      break;
    }
  }
  int has_allocations;
  if (curr_id == 0) { //scheduler
    has_allocations = scheduler_tcb->has_allocation;
  } else if (curr_id == 2) { // main TODO: what if we aren't running in a scheduler?
    has_allocations = main_tcb->has_allocation;
  } else {
    has_allocations = ((tcb*)get(all_threads, curr_id))->has_allocation;
  }
  prev_id = enter_mem_manager(curr_id);

//	printf("id: %d free call %s:%d %p\n",curr_id, file, line, p);
	int d = free_ptr(p, curr_id, has_allocations);
//	printf("free done\n");
	//	printMemory();
	if (d == 1) {
		error_message("Cannot free an already free pointer!", file, line);
	}
	else if (d==2) {
		error_message("This pointer was not malloc'd!", file, line);
	}
	exit_mem_manager(prev_id);
  exit_mem_manager(temp_id);
	exit_scheduler(&timer_pause_dump);
}
