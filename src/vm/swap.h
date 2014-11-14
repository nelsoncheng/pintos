#ifndef __VM_SWAP_H
#define __VM_SWAP_H

#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include <list.h>
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "devices/block.h"
#include <bitmap.h>
#include "userprog/pagedir.h"
#include "threads/vaddr.h"


struct block *swap_block
struct lock block_lock;
int size;
int page_to_sector_ratio;
struct bitmap *free_list;

struct swap_member{
  struct frame * frame_ptr;
  block_sector_t start_address;
}

// initializing our lists locks, and anything else
void swap_init();
#endif /* vm/swap.h */
