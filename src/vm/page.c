#include "vm/page.h"
#include "threads/malloc.h"

struct pte * page_new(page_type type, int offset, bool read_or_write, struct file * source_file, bool zero, size_t read_bytes, size_t zero_bytes){
  
  struct pte new_pte* = (struct pte *) malloc(sizeof(struct pte));
  if (new_pte == NULL){
    PANIC("Not enough memory to make a page table entry");
  }
  
  if (zero){
    new_pte->file_ptr = NULL;
    new_pte->file_offset = 0;
    new_pte->read_only = 0;
    new_pte->page_type = ZERO_PAGE;
    new_pte->bytes_to_read = 0;
    new_pte->bytes_to_zero = 0;
  } else {
    new_pte->file_ptr = source_file
    new_pte->file_offset = offset;
    new_pte->read_only = read_or_write;
    new_pte->page_type = type;
    new_pte->bytes_to_read = read_bytes;
    new_pte->bytes_to_zero = zero_bytes;
  }
  new_pte->frame_address = NULL;
  
  return new_pte;
}

struct pte* page_swap_pte(struct swap_member * swap){
  struct pte new_pte* = (struct pte *) malloc(sizeof(struct pte));
  if (new_pte == NULL){
    PANIC("Not enough memory to make a page table entry");
  }
  new_pte->page_type = SWAP_PAGE;
  new_pte->swap_member = swap;
  new_pte->file_ptr = NULL;
  new_pte->file_offset = NULL;
  new_pte->read_only = NULL;
  new_pte->bytes_to_read = 0;
  new_pte->bytes_to_zero = 0;
  return new_pte;
}

