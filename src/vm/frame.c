#include "vm/frame.h"

void frame_init(){
  list_init(&frame_list);
  lock_init(&frame_lock);
}

void * frame_get(void * vpage, bool zero_page, struct pte * sup_pte){
  void * kpage = palloc_get_page ( PAL_USER | (zero_page ? PAL_ZERO : 0) );
  
  if(kpage == NULL) {
    lock_acquire(&frame_lock);
    frame_evict();
    lock_release(&frame_lock);
    //PANIC ("no more frames, need to implement evict");
    return kpage;
  }
  
  struct frame * new_frame = (struct frame*) malloc (sizeof (struct frame));
  new_frame->paddr = kpage
  new_frame->upage = vpage
  new_frame->owner = thread_current();
  new_frame->pin = false;
  new_frame->saved_page = sup_pte;
  lock_acquire(&frame_lock);
  list_push_back(&frame_list, new_frame->elem);
  lock_release(&frame_lock);
  
  return kpage;
}

void frame_evict(){//FIFO evict
 struct list_elem iterator;
 struct frame * frame_ptr;
 struct pte * new_page;
 
 iterator = list_begin(&frame_list);
 while (iterator != list_end(&frame_list)){
   frame_ptr = list_entry(iterator, struct frame, elem);
   if (frame_ptr->pinned == false)
    break;
 }
 
 if (pagedir_is_dirty(frame_ptr->owner->pagedir, frame_ptr->upage)){//then the page was a stack or swap page
   //pin the frames here
   struct swap_member * swap = swap_insert(frame_ptr);
   //unpin the frames
   if (frame_ptr->saved_page == NULL){
     new_page = page_new(ZERO_PAGE, 0, 0, NULL, true, 0, 0);
   } else {
     new_page = page_swap_pte(swap);
   }
 } else {
   bool zero = frame_ptr->saved_page->ptype == ZERO_PAGE ? true : false;
   new_page = page_new(frame_ptr->saved_page->ptype, frame_ptr->saved_page->file_offset,
   frame_ptr->saved_page->read_only, frame_ptr->saved_page->file_ptr, zero,
   frame_ptr->saved_page->bytes_to_read, frame_ptr->saved_page->bytes_to_zero );
 }
 //possible need to use a semaphore for a thread's page directory here
 pagedir_clear_page (frame_ptr->owner->pagedir, frame_ptr->upage);
 pagedir_set_pte (frame_ptr->owner->pagedir, frame_ptr->upage, new_page);
 }  
