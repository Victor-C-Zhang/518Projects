#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include "open_address_ht.h"

void TEST_create_delete() {
  size_t SIZE = 2048;
  OpenAddrHashtable* ht = createTable(SIZE);
  assert(ht->_size == SIZE);
  for (int i = 0; i < 2048; ++i) {
    assert(ht->_table[i].key == HT_NULL_KEY && ht->_table[i].val ==
    HT_NULL_VAL);
  }
  deleteTable(ht);
}

void TEST_get_put() {
  size_t SIZE = 2048;
  OpenAddrHashtable* ht = createTable(SIZE);
  ht_val inserts[SIZE-1];
  for (int i = 0; i < SIZE - 1; ++i) {
    inserts[i] = i+1;
  }
  for (int i = 0; i < SIZE - 1; ++i) {
    srand(time(NULL));
    int randnum = rand()%(SIZE-1-i)+i;
    inserts[i]^=inserts[randnum];
    inserts[randnum]^=inserts[i];
    inserts[i]^=inserts[randnum];
  }
  // TODO: insert, validate
}