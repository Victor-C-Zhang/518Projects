#ifndef ASST2_OPEN_ADDRESS_HT_H
#define ASST2_OPEN_ADDRESS_HT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Table entries are represented in one 64-bit integer:
 * |----------pid----------|xxxxxkey|xxxval|
 * where x represents a pad bit.
 * The table is uses hash function
 *      h: <pid,key> \to [0,HT_SIZE)
 * where the input <pid,key> is a 48-bit integer padded to 64 bits.
 * pid    the 32-bit pid of the thread for which we store the key/val pair.
 * key    a 11-bit (padded to 16 bits) number representing the page ID in
 *        virtual address.
 * val    a 13-bit (padded to 16 bits) number representing where the page
 *        actually is, in physical memory or disk.
 */
#define HT_NULL_MASK 0x8000
#define HT_NULL_KEY 0x8000
#define HT_NULL_VAL 0x8000

typedef uint16_t ht_val;
typedef uint16_t ht_key;
typedef struct ht_entry_ {
  pid_t pid;
  ht_key key;
  ht_val val;
} ht_entry;
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
 * @param pid
 * @param key
 * @param val
 * @return the old value, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val ht_put(OpenAddrHashtable table, pid_t pid, ht_key key, ht_val val);

/**
 * Get the value associated with a key.
 * @param table
 * @param pid
 * @param key
 * @return the value, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val ht_get(OpenAddrHashtable table, pid_t pid, ht_key key);

/**
 * Remove the value associated with a key, if there is one. Raises an error
 * if there is no entry corresponding to the key.
 * @param table
 * @param pid
 * @param key
 * @throw error if the key does not correspond with a value.
 * @return the value deleted, if there is one, or HT_NULL_VAL otherwise.
 */
ht_val ht_delete(OpenAddrHashtable table, pid_t pid, ht_key key);

#endif //ASST2_OPEN_ADDRESS_HT_H
