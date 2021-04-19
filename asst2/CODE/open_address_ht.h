//
// Created by victor on 4/16/21.
//

#ifndef ASST1_OPEN_ADDRESS_HT_H
#define ASST1_OPEN_ADDRESS_HT_H

#include <stddef.h>
#include <stdint.h>
#include "types.h"


typedef ht_entry* OpenAddrHashtable;

/**
 * Create new open-addressed hashtable of size HT_SIZE at the given address.
 * @param ptr a pointer to the beginning of the table.
 */
void createTable(void* ptr);

/**
 * Put the key value pair into the hashtable. If there is already a value for
 * that key, replace it.
 * @param table
 * @param entry
 * @return the old value, if there is one, or HT_NULL_VAL otherwise.
 */

ht_val put(OpenAddrHashtable table, ht_entry entry);

/**
 * Get the value associated with a key.
 * @param table
 * @param getkey
 * @return the value, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val get(OpenAddrHashtable table, ht_key getkey);

/**
 * Remove the value associated with a key, if there is one. Raises an error
 * if there is no entry corresponding to the key.
 * @param table
 * @param delkey
 * @throw error if the key does not correspond with a value.
 * @return the value deleted, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val delete(OpenAddrHashtable table, ht_key delkey);

#endif //ASST1_OPEN_ADDRESS_HT_H
