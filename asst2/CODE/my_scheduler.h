#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

#include <signal.h>
#include <time.h>
#include "datastructs_t.h"
#include "my_pthread_t.h"
#include "my_malloc.h" 

#define STACKSIZE 32768
//#define STACKSIZE 1024
#define QUANTUM 25000000
#define ONE_SECOND 1000000000
#define NUM_QUEUES 5

typedef linked_list_t ready_q_t;

typedef enum thread_status{READY, DONE, BLOCKED} thread_status;

/* tcb struct definition */
typedef struct threadControlBlock {
  my_pthread_t id;
  ucontext_t context;
  void* ret_val;
  thread_status status;
  int priority; // number from 0 to NUM_QUEUES-1
  int cycles_left; // how many cycles the thread has left to run in its
  // timeslice
  int acq_locks; // how many locks the thread currently has
  uint64_t last_run; // cycle during which the thread was last run
  linked_list_t* waited_on; // linked-list of threads waiting on this thread
} tcb;

timer_t sig_timer;
static struct itimerspec timer_25ms = {
      .it_interval = {
            .tv_nsec = QUANTUM,
      },
      .it_value = {
            .tv_nsec = QUANTUM,
      }
};
static struct itimerspec timer_stopper = {};
struct itimerspec timer_pause_dump;

struct sigaction act;

ready_q_t* ready_q[NUM_QUEUES]; // ready queue, will be inited when scheduler created
ucontext_t* scheduler_context; // context that only runs the scheduler in a loop
int curr_prio; // priority of the currently scheduled thread. should usually
// be 0.
void* prev_done; // stack pointer of previously done context
int in_scheduler; // if scheduler is running
uint64_t cycles_run; // number of scheduling cycles run so far
int should_maintain; // run a maintenance cycle once
// value is <= 0
hashmap* all_threads; //all threads accessed by ids

/**
 * Enters scheduler context:
 * Pauses the alarm counter so the "scheduler" can work uninterrupted.
 * Unprotects scheduler-owned memory pages.
 * @param ovalue a pointer to store the current itimerspec in.
 */
void enter_scheduler(struct itimerspec* ovalue);

/**
 * Exits scheduler context:
 * Resumes the alarm counter.
 * Protects scheduler-owned memory pages.
 *
 * Must be the LAST thing done prior to returning control to user threads.
 * @param ovalue the itimerspec to resume.
 */
void exit_scheduler(struct itimerspec* ovalue);

/**
 * Will only be called via SIGALRM. (No need to set SA mask to ignore duplicate signal)
 * Saves context of currently running thread.
 * Moves thread to end of ready queue.
 * Sets context to head of ready queue.
 */
void alrm_handler(int sig, siginfo_t* info, void* ucontext);

/**
 * Utility function that actually does swapping. Will be called from a different
 * context as user threads, to allow swapping of user memory without issue.
 */
void schedule();

/**
 * Decay long-running procs.
 */
void run_maintenance();

/**
 * Insert a thread to the back of a queue
 * @param thread
 * @param queue_num the queue to insert to. 0 is highest priority.
 */
void insert_ready_q(tcb* thread, int queue_num);

/**
 * Swap in the relevant pages for the stack of the "new" process.
 * The new process stack will be unprotected. all other pages will retain
 * their protection status.
 */
int swap_stack(my_pthread_t new_id);

/**
 * Utility function to free data structures used by the scheduler. Called during
 * final teardown of the process.
 */
void free_data();
#endif
