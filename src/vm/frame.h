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
#include "vm/swap.h"
#include <stdbool.h>
#include <stdlib.h>

static struct list frame_list;
static struct lock frame_lock;
static struct list_elem * next_elem;

void frame_evict();
void frame_external_lock_acquire();
void frame_external_lock_release();
void * frame_get(void * vpage, bool zero_page, struct pte * sup_pte);
void frame_init();
bool frame_free (void * paddr);
bool frame_pin (void * address, bool user_or_kernal, bool pinval);


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
