#include "vm/page.h"
#include "threads/malloc.h"

struct pte * page_new(page_type type, int offset, bool read_or_write, struct file * source_file, bool zero){
  
  struct pte new_pte* = (struct pte *) malloc(sizeof(struct pte));
  if (new_pte == NULL){
    PANIC("Not enough memory to make a page table entry");
  }
  
  if (zero){
    new_pte->file_ptr = NULL;
    new_pte->file_offset = NULL;
    new_pte->read_only = NULL;
    new_pte->page_type = ZERO_PAGE;
  } else {
    new_pte->file_ptr = source_file
    new_pte->file_offset = offset
    new_pte->read_only = read_or_write;
    new_pte->page_type = type;
  }
  new_pte->frame_address = NULL;
  
  return new_pte;
}

bool page_is_stack_fault(void * stack_ptr, void * fault_address){
  //TODO: devise a way to determine if a page fault's faulting address was meang to be a stack access
}
