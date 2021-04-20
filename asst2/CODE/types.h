#ifndef ASST1_TYPES_H
#define ASST1_TYPES_H

/*
 * Keys/values are represented in one 64-bit integer. A key consists of a
 * 32-bit uint representing the PID, and a 11-bit (padded to 16 bits) number
 * representing where the enquirer thinks the page ID is. A value is a 13-bit
 * (padded to 16 bits) number representing where it actually is, in physical
 * memory or disk.
 * The key/value is stored as
 * |----------PID----------|xxxxxQueryID|xxxActualID|
 * where x represents a pad bit.
 */
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>

#define HT_NULL_VAL_MASK 0x8000 // top bit of ActualID set to 1
#define HT_NULL_KEY_MASK 0x80000000 // top bit of QueryID set to 1

const static uint64_t val_mask = 0xFFFF;
const static uint64_t query_mask = 0xFFFF0000;
const static uint64_t pid_mask = 0xFFFFFFFF00000000;
const static uint64_t key_mask = 0xFFFFFFFFFFFF0000;

typedef struct _ht_entry {
  uint64_t mdata;
} ht_entry;
typedef struct _ht_query {
  uint16_t mdata;
} ht_query;
typedef struct _ht_key {
  uint64_t mdata;
} ht_key;
typedef struct _ht_val {
  uint16_t mdata;
} ht_val;

static inline pid_t pid(ht_entry entry) { return entry.mdata >> 32; }
static inline ht_key key(ht_entry entry) {
  ht_key retval;
  retval.mdata = (entry.mdata & key_mask) >> 16;
  return retval;
}
static inline ht_query query(ht_entry entry) {
  ht_query retval;
  retval.mdata = (entry.mdata & query_mask) >> 16;
  return retval;
}
static inline ht_val val(ht_entry entry) {
  ht_val retval;
  retval.mdata = entry.mdata & val_mask;
  return retval;
}
static inline ht_entry entry(pid_t pid, ht_query query, ht_val val) {
  ht_entry retval;
  retval.mdata = ((uint64_t)pid << 32) | (query.mdata << 16) | val.mdata;
  return retval;
}

int MEM_LIMIT;
int VIRT_LIMIT;
int HT_SIZE;

#endif //ASST1_TYPES_H
