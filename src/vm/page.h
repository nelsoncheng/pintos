#ifndef __VM_PAGE_H
#define __VM_PAGE_H



struct page{
  // owner of the page
  struct thread* owner;
  // user page virtual address
  void *upage;
  // file associated with the process
  struct file *fileptr;
  // kernel page
  void *kpage;
  // list for holding pages
  struct list_elem elem;
};








#endif /* vm/page.h */
