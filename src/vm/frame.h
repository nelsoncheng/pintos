#ifndef __VM_FRAME_H
#define __VM_FRAME_H

#include <stdio.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include <stdbool.h>
#include <stdlib.h>

static struct list frame_list;
static struct lock frame_lock;

void * frame_get(void * vpage, bool zero_page);
void frame_init();

struct frame{
  // physical address location
  void* paddr;
  // location at virtual address space
  void* upage;
  // who owns this frame
  struct thread* owner;
  // if the frame is pinned
  bool pin;
  // for storing it in a list if we need
  struct list_elem elem;
  //when a page gets paged in to physical memory, store its supplemental page here in case it later on gets evicted
  struct pte* saved_page;
};













#endif /* vm/frame.h */
