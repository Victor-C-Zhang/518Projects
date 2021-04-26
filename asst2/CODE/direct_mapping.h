#ifndef ASST2_DIRECT_MAPPING_H
#define ASST2_DIRECT_MAPPING_H

#include <stdint.h>
#include <stddef.h>

#define NOT_OCC 0
#define OCC 1
#define NOT_OVF 0
#define OVF 1
#define LAST 1
#define NOT_LAST 0
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

/*
 * Pagedata consists of:
 * pid        a 32-bit value representing the process owning the page
 * 
 * p_ind      a 16-bit value arranged as follows: |f|o|index|
 *
 * f          a 1-bit flag denoting whether the page is occupied. 1 for
 *            occupied, 0 for free.
 *
 * o          a 1-bit flag denoting whether the page's allocation overflows to
 *            the next page.
 *
 * index      a 11-bit number representing the index where the process thinks 
 *	      this page is.
 *
 * length     a 16-bit number representing how many contiguous pages the allocations
 * 	      in the current page span.
 */
struct _pagedata_ {
	uint32_t pid;
	unsigned short p_ind;
	unsigned short length;
} typedef pagedata;

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

//if the page pointed to by curr is occupied
int pg_block_occupied(pagedata* curr);

//if the page pointed to by curr overflows to next page
int pg_is_overflow(pagedata* curr);

//returns the index where the process owning the page
//thinks the page is locate at
int pg_index(pagedata* curr) {

/**
 * Writes a given configuration to the metadata byte pointed by curr.
 * @param curr      the pointer.
 * @param pid       the process owning the page.
 * @param occ       whether the page is free or occupied.
 * @param overf     whether the page overflows to the next page.
 * @param ind       index of where the process thinks this page is located.
 * @param len       the number of contiguous pages allocations in the curr span.
 */
void pg_write_pagedata(pagedata* curr, uint32_t pid, int occ, int overf, unsigned short ind, unsigned short len);

#endif //ASST2_DIRECT_MAPPING_H
