//
// Created by victor on 4/16/21.
//

#ifndef ASST1_OPEN_ADDRESS_HT_H
#define ASST1_OPEN_ADDRESS_HT_H

#include "types.h"

typedef struct _ht_entry {
  ht_key key;
  ht_val val;
} HashEntry;

typedef struct _linear_probe {
  size_t _size;
  HashEntry* _table;
} OpenAddrHashtable;

/**
 * Create new open-addressed hashtable.
 * @param size the initial capacity.
 * @return a pointer representing the table.
 */
OpenAddrHashtable* createTable(size_t size = 128);

/**
 * Delete the provided table.
 * @param table
 */
void deleteTable(OpenAddrHashtable* table);

/**
 * Put the key value pair into the hashtable. If there is already a value for
 * that key, replace it.
 * @param table
 * @param key
 * @param val
 * @return the old value, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val put(OpenAddrHashtable* table, ht_key key, ht_val val);

/**
 * Get the value associated with a key.
 * @param table
 * @param key
 * @return the value, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val get(OpenAddrHashtable* table, ht_key key);

#endif //ASST1_OPEN_ADDRESS_HT_H
