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

#define HT_NULL_VAL 0x8000 // top bit of ActualID set to 1
#define HT_NULL_KEY 0x80000000 // top bit of QueryID set to 1

const static uint64_t val_mask = 0xFFFF;
const static uint64_t query_mask = 0xFFFF0000;
const static uint64_t pid_mask = 0xFFFFFFFF00000000;
const static uint64_t key_mask = 0xFFFFFFFFFFFF0000;

typedef uint64_t ht_entry;
typedef uint16_t ht_query;
typedef uint64_t ht_key;
typedef uint16_t ht_val;

inline pid_t pid(ht_entry entry) { return entry >> 32; }
inline ht_key key(ht_entry entry) { return (entry & key_mask) >> 16; }
inline ht_query query(ht_entry entry) { return (entry & query_mask) >> 16; }
inline ht_val val(ht_entry entry) { return (entry & val_mask); }
inline ht_entry entry(pid_t pid, ht_query query, ht_val val) {
  return ((uint64_t)pid << 32) | (query << 16) | val;
}

static int MEM_LIMIT;
static int VIRT_LIMIT;
static int HT_SIZE;

#endif //ASST1_TYPES_H
