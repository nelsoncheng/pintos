#ifndef __VM_FRAME_H
#define __VM_FRAME_H

#include <stdio.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"

static struct list frame_list;
static struct lock frame_lock;

struct frame{
  // physical address location
  void* paddr;
  // location at virtual address space
  void* upage;
  // who owns this frame
  struct thread* owner;
  // used for swapping, assign counter during creation?
  int recent;
  // for storing it in a list if we need
  struct list_elem elem;
  
};













#endif /* vm/frame.h */
