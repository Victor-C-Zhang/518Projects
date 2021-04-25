#ifndef ASST2_DIRECT_MAPPING_H
#define ASST2_DIRECT_MAPPING_H

#include <stdint.h>
#include <stddef.h>

/*
 * Metadata consists of 8 bits arranged as follows:
 * |f|l|norm-size|
 * f          a 1-bit flag denoting whether the segment is occupied. 1 for
 *            occupied, 0 for free.
 *
 * l          a 1-bit flag denoting whether the segment is the last segment in a
 *            page.
 *
 * norm-size  a 6-bit number representing the "normalized" size of a segment.
 *            A normalized size of n represents an actual segment size of n+1.
 */
struct metadata_ {
  uint8_t size;
} typedef metadata;

// if the segment pointed by curr is occupied
int dm_block_occupied(metadata* curr);

// if curr points to the last segment in a page
int dm_is_last_segment(metadata* curr);

// the (de-normalized) length of the block curr points to
uint8_t dm_block_size(metadata* curr);


/**
 * Writes a given configuration to the metadata byte pointed by curr.
 * @param curr      the pointer.
 * @param size      the length of segment.
 * @param occupied  whether the segment is free or occupied.
 * @param last      whether the segment is the last segment in the page.
 */
void dm_write_metadata(metadata* curr, size_t size, int occupied, int last);

/**
 * Allocate a segment at the pointed free block. Guaranteed to split the
 * block if necessary.
 * @param curr
 * @param size
 */
void dm_allocate_block(metadata* curr, size_t size);

#endif //ASST2_DIRECT_MAPPING_H
