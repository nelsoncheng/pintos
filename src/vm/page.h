#ifndef __VM_PAGE_H
#define __VM_PAGE_H


struct pte{
  struct file *fileptr;
  // the byte we're at in the current file
  int file_offset;
  // tells if the page is read only or not
  bool read_only;
  //tells what kind of page this is 
  int page_type;
  
  //if this is NULL then the frame isnt in physical memory,
  //otherwise tells which kernal address points to this page
  void * frame_address;
};








#endif /* vm/page.h */
