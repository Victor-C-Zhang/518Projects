#include "direct_mapping.h"
#include "global_vals.h"

int dm_block_occupied(metadata* curr) {
  return curr->size & 0x80;
}

int dm_is_last_segment(metadata* curr) {
  return (curr->size & 0x40) >> 6;
}

uint8_t dm_block_size(metadata* curr) {
  return (curr->size & 0x3f) + 1;
}

void dm_write_metadata(metadata* curr, size_t size, int occupied, int last) {
  curr -> size = ((uint8_t)(size-1)) | (occupied << 7) | (last << 6);
}

void dm_allocate_block(metadata* curr, size_t size) {
  uint8_t curr_seg = dm_block_size(curr);
  int is_last_segment = dm_is_last_segment(curr);
  if (size < curr_seg) {
    dm_write_metadata(curr, size, 1, 0);
    dm_write_metadata(curr+size*SEGMENTSIZE, curr_seg-size, 0, is_last_segment);
  } else {
    dm_write_metadata(curr, size, 1, is_last_segment);
  }
}