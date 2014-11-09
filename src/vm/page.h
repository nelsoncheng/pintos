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
};








#endif /* vm/page.h */
