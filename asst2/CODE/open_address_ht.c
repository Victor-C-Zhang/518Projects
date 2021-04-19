#include <stdlib.h>
#include <stdio.h>
#include "open_address_ht.h"

void createTable(void* ptr) {
  ht_entry* table = ptr;
  for (int i = 0; i < HT_SIZE; ++i) {
    table[i] = HT_NULL_KEY | HT_NULL_VAL;
  }
}

ht_val put(OpenAddrHashtable table, ht_entry entry) {
  int hash = (key(entry) % HT_SIZE);
  while (table[hash] & HT_NULL_VAL && key(table[hash]) != hash)
    hash = (hash + 1) % HT_SIZE;
  ht_val oldval = val(table[hash]);
  table[hash] = entry;
  return oldval;
}

ht_val get(OpenAddrHashtable table, ht_key getkey) {
  int hash = (getkey % HT_SIZE);
  while (table[hash] & HT_NULL_VAL && key(table[hash]) != getkey)
    hash = (hash + 1) % HT_SIZE;
  return val(table[hash]);
}

ht_val delete(OpenAddrHashtable table, ht_key delkey) {
  int hash = (delkey % HT_SIZE);
  while (table[hash] & HT_NULL_VAL) {
    if (table[hash] & HT_NULL_KEY) {
      perror("No value exists for this pid,key combination");
      exit(EXIT_FAILURE);
    }
    hash = (hash + 1) % HT_SIZE;
  }
  ht_val retval = val(table[hash]);
  table[hash] |= val_mask;
  return retval;
}