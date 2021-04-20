#include <stdlib.h>
#include <stdio.h>
#include "open_address_ht.h"

void createTable(void* ptr) {
  MEM_LIMIT = 8388608/sysconf(_SC_PAGESIZE);
  VIRT_LIMIT = 25165824/sysconf(_SC_PAGESIZE);
  HT_SIZE = MEM_LIMIT;
  ht_entry* table = ptr;
  for (int i = 0; i < HT_SIZE; ++i) {
    table[i] = (ht_entry) {HT_NULL_KEY_MASK | HT_NULL_VAL_MASK};
  }
}

ht_val ht_put(OpenAddrHashtable table, ht_entry entry) {
  int hash = (key(entry).mdata % HT_SIZE);
  while (!(table[hash].mdata & HT_NULL_VAL_MASK) && key(table[hash]).mdata != hash)
    hash = (hash + 1) % HT_SIZE;
  ht_val oldval = val(table[hash]);
  table[hash] = entry;
  return oldval;
}

ht_val ht_get(OpenAddrHashtable table, ht_key getkey) {
  int hash = (getkey.mdata % HT_SIZE);
  while (!(table[hash].mdata & HT_NULL_KEY_MASK) && key(table[hash]).mdata != getkey.mdata)
    hash = (hash + 1) % HT_SIZE;
  return val(table[hash]);
}

ht_val ht_delete(OpenAddrHashtable table, ht_key delkey) {
  int hash = (delkey.mdata % HT_SIZE);
  while (key(table[hash]).mdata != delkey.mdata) {
    if (table[hash].mdata & HT_NULL_KEY_MASK) {
      printf("Tried to delete pid:%lu, key:%lu\n",delkey.mdata>>16,delkey.mdata&val_mask);
      perror("No value exists for this pid,key combination");
      exit(EXIT_FAILURE);
    }
    hash = (hash + 1) % HT_SIZE;
  }
  ht_val retval = val(table[hash]);
  if (key(table[(hash+1)%HT_SIZE]).mdata == HT_NULL_KEY_MASK) {
    // no need to retain tombstone at the end of a run
    table[hash].mdata = HT_NULL_KEY_MASK | HT_NULL_VAL_MASK;
  } else {
    table[hash].mdata |= val_mask;
  }

  // TODO: exhume the graveyard once in a while
  return retval;
}