#ifndef MY_SCHEDULER_T_H
#define MY_SCHEDULER_T_H

#include <signal.h>
#include <time.h>
#include "datastructs_t.h"
#include "my_pthread_t.h"

#define STACKSIZE 32768
#define QUANTUM 25000000
#define ONE_SECOND 1000000000
#define NUM_QUEUES 5

typedef linked_list_t ready_q_t;

typedef enum thread_status{READY, DONE, BLOCKED} thread_status;


/* tcb struct definition */
typedef struct threadControlBlock {
  uint32_t id;
  ucontext_t* context;
  void* ret_val;
  thread_status status;
  int priority; // number from 0 to NUM_QUEUES-1
  int cycles_left; // how many cycles the thread has left to run in its
  // timeslice
  int acq_locks; // how many locks the thread currently has
  uint64_t last_run; // cycle during which the thread was last run
  linked_list_t* waited_on; // linked-list of threads waiting on this thread
} tcb;

timer_t* sig_timer;
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

struct sigaction* act;

ready_q_t* ready_q[NUM_QUEUES]; // ready queue, will be inited when scheduler created
int curr_prio; // priority of the currently scheduled thread. should usually
// be 0.
ucontext_t* prev_done;
int in_scheduler; // if scheduler is running
uint64_t cycles_run; // number of scheduling cycles run so far
int should_maintain; // run a maintenance cycle once
// value is <= 0
hashmap* all_threads; //all threads accessed by ids

/**
 * Pauses the alarm counter so the "scheduler" can work uninterrupted.
 * @param ovalue a pointer to store the current itimerspec in.
 */
void enter_scheduler(struct itimerspec* ovalue);

/**
 * Resumes the alarm counter. Must be the LAST thing done prior to returning
 * control to user threads.
 * @param ovalue the itimerspec to resume.
 */
void exit_scheduler(struct itimerspec* ovalue);

/**
 * Will only be called via SIGALRM. (No need to set SA mask to ignore duplicate signal)
 * Saves context of currently running thread.
 * Moves thread to end of ready queue.
 * Sets context to head of ready queue.
 */
void schedule(int sig, siginfo_t* info, void* ucontext);

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

#endif
