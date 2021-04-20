#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include "open_address_ht.h"
#include "test_runner.h"

void TEST_create() {
  OpenAddrHashtable ht = malloc(HT_SIZE*sizeof(ht_entry));
  createTable(ht);
  for (int i = 0; i < HT_SIZE; ++i) {
    assert((ht[i].mdata & val_mask) == HT_NULL_VAL_MASK);
    assert((ht[i].mdata & key_mask) == HT_NULL_KEY_MASK);
  }
  free(ht);
}

void TEST_get_put_delete() {
  OpenAddrHashtable ht = malloc(HT_SIZE*sizeof(ht_entry));
  createTable(ht);
  ht_entry inserts[HT_SIZE/2];
  // insert in order
  for (int i = 0; i < HT_SIZE/2; ++i) {
    inserts[i] = entry(i,(ht_query){i%1024},(ht_val){(i*101)%1024});
  }
  for (int i = HT_SIZE/2; i > 1; --i) {
    srand(14);
    int randnum = rand()%i;
    inserts[0].mdata^=inserts[randnum].mdata;
    inserts[randnum].mdata^=inserts[0].mdata;
    inserts[0].mdata^=inserts[randnum].mdata;
  }
  for (int i = 0; i < HT_SIZE/2; ++i) {
    ht_put(ht, inserts[i]);
  }
  for (int i = 0; i < HT_SIZE/2; ++i) {
    assert(ht_get(ht, key(inserts[i])).mdata == val(inserts[i]).mdata);
  }
  for (int i = 0; i < HT_SIZE / 2; ++i) {
    ht_delete(ht, key(inserts[i]));
  }

  ht_entry collision_inserts[7];
  for (int i = 0; i < 7; ++i) {
    collision_inserts[i] = entry(1,(ht_query){HT_SIZE*i+69},(ht_val){i});
  }
  for (int i = 0; i < 5; ++i) {
    ht_put(ht, collision_inserts[i]);
  }
  ht_delete(ht, key(collision_inserts[1]));
  ht_delete(ht, key(collision_inserts[2]));
  for (int i = 5; i < 7; ++i) {
    ht_put(ht, collision_inserts[i]);
  }
  assert(ht_get(ht, key(collision_inserts[0])).mdata == val(collision_inserts[0]).mdata);
  for (int i = 3; i < 7; ++i) {
    assert(ht_get(ht, key(collision_inserts[i])).mdata == val(collision_inserts[i]).mdata);
  }
}
