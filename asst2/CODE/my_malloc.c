#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "my_malloc.h"

char* MEM = NULL;

struct sigaction* act;

static void handler(int sig, siginfo_t* si, void* unused) {
  printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
}

/**
 * Frees data allocated by memory management system during process teardown.
 */
void free_mem_metadata() {
  free(act);
  // oops, we probably can't do this
  free(MEM);
}

void* myallocate(size_t size) {
  if (MEM == NULL) { // first time using malloc
    atexit(free_mem_metadata);
    MEM = memalign(sysconf(_SC_PAGE_SIZE), 8388608);

    act = malloc(sizeof(struct sigaction));
    act->sa_sigaction = handler;
    act->sa_flags = SA_SIGINFO;
    sigemptyset(&act->sa_mask);
    sigaddset(&act->sa_mask,SIGALRM); // block scheduling attempts while
    // resolving memory issues
    sigaction(SIGSEGV, act, NULL);

    // do the rest of allocation here
  }

  // TODO: placeholder
  return NULL;
}