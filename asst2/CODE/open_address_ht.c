#include "open_address_ht.h"

OpenAddrHashtable* createTable(size_t size) {
  OpenAddrHashtable* ret = malloc(sizeof(OpenAddrHashtable));
  ret->_table = malloc(size*sizeof(HashEntry));
  for (int i = 0; i < size; ++i) {
    ret->_table[i] = {HT_NULL_KEY,HT_NULL_VAL};
  }
  return ret;
}

void deleteTable(OpenAddrHashtable* table) {
  free(table->_table);
  free(table);
}

ht_val put(OpenAddrHashtable* table, ht_key key, ht_val val) {
  int hash = (key % table->_size);
  while (table->_table[hash] != HT_NULL_KEY && table->_table[hash].key != key)
    hash = (hash + 1) % table->_size;
  ht_val oldval = table->_table[hash].val;
  table->_table[hash] = {key,val};
  return oldval;
}

ht_val get(OpenAddrHashtable* table, ht_key key);
  int hash = (key % table->_size);
  while (table->_table[hash] != HT_NULL_KEY && table->_table[hash]key != key)
    hash = (hash + 1) % table->_size;
  return table->_table[hash].val;
}