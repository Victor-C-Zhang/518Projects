#include <assert.h>
#include <stdlib.h>
#include "open_address_ht.h"
#include "global_vals.h"
#include "test_runner.h"

// hacky fix to ensure tests run separately
uint64_t arr[2048];

void TEST_create() {
  OpenAddrHashtable ht = (ht_entry*) arr;
  createTable(ht);
  for (int i = 0; i < HT_SIZE; ++i) {
    assert((ht[i].val) == HT_NULL_VAL);
    assert((ht[i].key) == HT_NULL_KEY);
  }
}

void swap(ht_entry* first, ht_entry* second) {
  first->pid ^= second->pid;
  second->pid ^= first->pid;
  first->pid ^= second->pid;

  first->key ^= second->key;
  second->key ^= first->key;
  first->key ^= second->key;

  first->val ^= second->val;
  second->val ^= first->val;
  first->val ^= second->val;
}

void TEST_get_put_delete() {
  OpenAddrHashtable ht = (ht_entry*) arr;
  createTable(ht);
  ht_entry inserts[HT_SIZE/2];
  // insert in order
  for (int i = 0; i < HT_SIZE/2; ++i) {
    inserts[i] = (ht_entry) {i,(ht_key)(i%1024),(ht_val)((i*101)%1024)};
  }
  for (int i = HT_SIZE/2; i > 1; --i) {
    srand(14);
    int randnum = rand()%i;
    swap(&inserts[0],&inserts[randnum]);
  }
  for (int i = 0; i < HT_SIZE/2; ++i) {
    ht_put(ht, inserts[i].pid, inserts[i].key, inserts[i].val);
  }
  for (int i = 0; i < HT_SIZE/2; ++i) {
    assert(ht_get(ht,inserts[i].pid, inserts[i].key) == inserts[i].val);
  }
  for (int i = 0; i < HT_SIZE / 2; ++i) {
    ht_delete(ht, inserts[i].pid, inserts[i].key);
  }

  ht_entry collision_inserts[7];
  for (int i = 0; i < 7; ++i) {
    collision_inserts[i] = (ht_entry){1,(ht_key)(HT_SIZE*i+69),(ht_val)(i)};
  }
  for (int i = 0; i < 5; ++i) {
    ht_put(ht, collision_inserts[i].pid, collision_inserts[i].key,
           collision_inserts[i].val);
  }
  ht_delete(ht, collision_inserts[1].pid, collision_inserts[1].key);
  ht_delete(ht, collision_inserts[2].pid, collision_inserts[2].key);
  for (int i = 5; i < 7; ++i) {
    ht_put(ht, collision_inserts[i].pid, collision_inserts[i].key,
           collision_inserts[i].val);
  }
  assert(ht_get(ht, collision_inserts[0].pid, collision_inserts[0].key) ==
        collision_inserts[0].val);
  for (int i = 3; i < 7; ++i) {
    assert(ht_get(ht, collision_inserts[i].pid, collision_inserts[i].key) ==
          collision_inserts[i].val);
  }
}
