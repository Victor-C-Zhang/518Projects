#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "open_address_ht.h"
#include "global_vals.h"

int lin_hash(pid_t pid, ht_key key) {
  return (((uint64_t)pid << 16) | (uint64_t) key) % HT_SIZE;
}

void createTable(void* ptr) {
  MEM_LIMIT = MEMSIZE/sysconf(_SC_PAGESIZE);
  VIRT_LIMIT = VIRTSIZE/sysconf(_SC_PAGESIZE);
  HT_SIZE = VIRT_LIMIT;
  ht_entry* table = ptr;
  for (int i = 0; i < HT_SIZE; ++i) {
    table[i] = (ht_entry) {0,HT_NULL_KEY, HT_NULL_VAL};
  }
}

ht_val ht_put(OpenAddrHashtable table, pid_t pid, ht_key key, ht_val val) {
  int hash = lin_hash(pid,key);
  while ((table[hash].pid != pid || table[hash].key != key)
        && !(table[hash].val & HT_NULL_MASK))
    hash = (hash + 1) % HT_SIZE;
  ht_val oldval = table[hash].val;
  table[hash] = (ht_entry) {pid,key,val};
  return oldval;
}

ht_val ht_get(OpenAddrHashtable table, pid_t pid, ht_key key) {
  int hash = lin_hash(pid,key);
  while ((table[hash].pid != pid || table[hash].key != key)
        && !(table[hash].key & HT_NULL_MASK))
    hash = (hash + 1) % HT_SIZE;
  return table[hash].val;
}

ht_val ht_delete(OpenAddrHashtable table, pid_t pid, ht_key key) {
  int hash = lin_hash(pid,key);
  while (table[hash].pid != pid || table[hash].key != key) {
    if (table[hash].key & HT_NULL_MASK) {
      printf("Tried to delete pid:%d, key:%x\n",pid,key);
      perror("No value exists for this pid,key combination");
      exit(EXIT_FAILURE);
    }
    hash = (hash + 1) % HT_SIZE;
  }
  ht_val retval = table[hash].val;
  if (table[(hash+1)%HT_SIZE].key == HT_NULL_MASK) {
    // no need to retain tombstone at the end of a run
    table[hash] = (ht_entry) {0,HT_NULL_KEY,HT_NULL_VAL};
  } else {
    table[hash].val = HT_NULL_VAL;
  }

  // TODO: exhume the graveyard once in a while
  return retval;
}
