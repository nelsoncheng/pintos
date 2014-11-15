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


//Nelson coding
struct block *swap_block;
struct lock block_lock;
int size;
int page_to_sector_ratio;
struct bitmap *free_list;

// initializing our lists locks, and anything else
void swap_init();
struct swap_member * swap_insert(struct frame *);
void swap_retrieve(struct swap_member *, void *);

#endif
