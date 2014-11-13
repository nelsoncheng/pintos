#ifndef __VM_PAGE_H
#define __VM_PAGE_H

typedef enum {MMAP_FILE_PAGE, EXECUTABLE_PAGE, SWAP_PAGE, ZERO_PAGE} page_type;

struct pte * page_new(page_type type, int offset, bool read_or_write, struct file * source_file, bool zero);

struct pte{
  struct file *fileptr;
  // the byte we're at in the current file
  int file_offset;
  // tells if the page is read only or not
  bool read_only;
  //tells what kind of page this is 
  page_type ptype;
  
  //if this is NULL then the frame isnt in physical memory,
  //otherwise tells which kernal address points to this page
  void * frame_address;
};








#endif /* vm/page.h */
