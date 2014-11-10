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
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

// initializing our lists locks, and anything else
void swap_init(void);
// finds the page in the frames with the specified upage 
struct page *swap_find (void *upage);
// replaces the page with new page
void* swap_evict(void *upage, page* npage)

















#endif /* vm/swap.h */
