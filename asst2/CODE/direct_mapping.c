#include "direct_mapping.h"
#include "global_vals.h"

int dm_block_occupied(metadata* curr) {
  return (curr->size & 0x80) >> 7;
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

void* dm_allocate_block(my_pthread_t curr_id, int curr_page_num, int
curr_seg_num, int space, int req_size) {
  if (ht_get(ht_space, curr_id, curr_page_num) != curr_page_num) {
    printf("BIG ERROR: PAGE METADATA OUT OF SYNC (1)\n");
    exit(1);
  }
  metadata* curr = (metadata *) mem_space + curr_page_num * page_size +
                   curr_seg_num * SEGMENTSIZE;
  pagedata* curr_page = (pagedata*) myblock + curr_page_num;
  metadata* space_ptr = curr + SEGMENTSIZE*req_size;

  if (space == req_size) { // just flip a bit 4head
    size_t size = dm_block_size(curr);
    int isLast = dm_is_last_segment(curr);
    dm_write_metadata(curr, size, OCC, isLast);
    return curr + 1;
  }
  if (space + curr_seg_num <= num_segments) { // just phase A
    uint8_t curr_seg = dm_block_size(curr);
    int is_last_segment = dm_is_last_segment(curr);
    if (req_size < curr_seg) {
      dm_write_metadata(curr, req_size, OCC, NOT_LAST);
      dm_write_metadata(space_ptr, curr_seg-req_size, NOT_OCC,
                        is_last_segment);
    } else {
      dm_write_metadata(curr, req_size, OCC, is_last_segment);
    }
    return curr + 1;
  }
  // otherwise space definitely overflows onto another page
  if (curr_seg_num + req_size < num_segments) { // space starts at current page
    dm_write_metadata(curr, req_size, OCC, NOT_LAST);
    dm_write_metadata(space_ptr, (space-req_size)%num_segments + 1,
                      NOT_OCC, LAST);
    pg_write_pagedata(curr_page, curr_id, OCC, OVF, curr_page_num, (space -
    req_size)/num_segments);
    return curr + 1;
  }
  // otherwise space starts on another page...
  // first write data for requested alloc
  if (curr_seg_num + req_size == num_segments) { // no overflow of requested alloc
    dm_write_metadata(curr, req_size, OCC, LAST);
    pg_write_pagedata(curr_page, curr_id, OCC, NOT_OVF, curr_page_num, 1);
  } else { // boo! requested alloc also overflows
    dm_write_metadata(curr, req_size%num_segments + 1, OCC, LAST);
    pg_write_pagedata(curr_page, curr_id, OCC, OVF, curr_page_num,
                      req_size/num_segments);
  }
  // now write data for leftover
  int space_page_offset = (curr_seg_num + req_size) / num_segments;
  int ovf_amt = (curr_seg_num + req_size) % num_segments;
  int leftover = space - req_size;
  if (ht_get(ht_space, curr_id, space_page_offset + curr_page_num) !=
      curr_page_num + space_page_offset) {
    printf("BIG ERROR: PAGE METADATA OUT OF SYNC (3)\n");
    exit(1);
  }
  if (leftover + ovf_amt < num_segments) { // space takes part of one page
    dm_write_metadata(space_ptr, leftover, NOT_OCC, NOT_LAST);
  } else if (leftover + ovf_amt == num_segments) { // space takes whole page
    dm_write_metadata(space_ptr, leftover, NOT_OCC, LAST);
    pg_write_pagedata(curr_page + space_page_offset, curr_id, OCC, NOT_OVF,
                      curr_page_num + space_page_offset, 1);
  } else { // space overflows onto next page
    dm_write_metadata(space_ptr, leftover%num_segments + 1, NOT_OCC, LAST);
    pg_write_pagedata(curr_page + space_page_offset, curr_id, OCC, OVF,
                      curr_page_num + space_page_offset, leftover/num_segments);
  }
  return curr + 1;
}

int pg_block_occupied(pagedata* curr) {
	return  (curr->p_ind & 0x8000) >> 15;
}

int pg_is_overflow(pagedata* curr) {
	return (curr->p_ind & 0x4000) >> 14;
}

int pg_index(pagedata* curr) {
	return (curr->p_ind & 0x3fff);
}

void pg_write_pagedata(pagedata* curr, my_pthread_t pid, int occ, int overf, unsigned
short ind, unsigned short len) {
  curr->pid = pid;
  curr->p_ind = (ind & 0x7fff) | (occ << 15) | (overf << 14);
  curr->length = len;
}
