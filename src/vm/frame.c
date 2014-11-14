#include "vm/frame.h"

void frame_init(){
  list_init(&frame_list);
  lock_init(&frame_lock);
  next_elem = list_begin(&frame_list);
}

void frame_external_lock_acquire(){
  lock_acquire(&frame_lock);
}

void frame_external_lock_release(){
  lock_release(&frame_lock);
}

void * frame_get(void * vpage, bool zero_page, struct pte * sup_pte){
  void * kpage = palloc_get_page ( PAL_USER | (zero_page ? PAL_ZERO : 0) );
  
  if(kpage == NULL) {
    lock_acquire(&frame_lock);
    frame_evict();
    kpage = palloc_get_page ( PAL_USER | (zero_page ? PAL_ZERO : 0) );
    lock_release(&frame_lock);
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
 struct list_elem * iterator;
 struct frame * frame_ptr, * true_page;
 struct pte * new_page;
 int i;
 
 /* fifo
 iterator = list_begin(&frame_list);
 while (iterator != list_end(&frame_list)){
   frame_ptr = list_entry(iterator, struct frame, elem);
   if (frame_ptr->pinned == false)//need to actually implement pinning
    break;
  iterator = list_next(iterator);
 }
 */
 //clock
 iterator = next_elem;
 for (i = 0; i < 2 && true_page == NULL; i++){
   while (iterator != list_end(&frame_list)){
     frame_ptr = list_entry(iterator, struct frame, elem);
     bool accessed = pagedir_is_accessed(frame_ptr->owner->pagedir, frame_ptr->upage)
     if (frame_ptr->pinned == false && !accessed){//need to actually implement pinning
      true_page = frame_ptr;
      break;
     } else if (accessed){
      pagedir_set_accessed(f->owner->pagedir, f->upage, false);
     }
     iterator = list_next(iterator);
   }
 }
 next_elem = list_next(iterator);
 if (true_page == NULL){
   printf("all pages were pinned??\n");
 }
 //starting from here comes the actual eviction
 if (pagedir_is_dirty(frame_ptr->owner->pagedir, frame_ptr->upage)){//then the page was a stack or swap page
   //pin the frames here
   frame_pin(frame_ptr->upage, false, true);
   struct swap_member * swap = swap_insert(frame_ptr);
   frame_pin(frame_ptr->upage, false, false);
   //unpin the frames
   if (frame_ptr->saved_page == NULL){
     new_page = page_new(ZERO_PAGE, 0, 0, NULL, true, 0, 0);
     free(swap);
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
 
 list_remove(iterator);
 palloc_free_page(frame_ptr->paddr);
 free(frame_ptr);
 
 
  
}  
 


bool frame_free (void * paddr){
  struct list_elem iterator;
  struct frame * frame_ptr;
  
  lock_acquire(&frame_lock);
  
  iterator = list_begin(&frame_list);
  while (iterator != list_end(&frame_list)){
    frame_ptr = list_entry(iterator, struct frame, elem);
    if (frame_ptr->paddr == paddr){
      break;
   }
   iterator = list_next(iterator);
  }
  palloc_free_page(frame_ptr->paddr);
  list_remove(iterator);
  free(frame_ptr);
  
  lock_release(&frame_lock);
 }
 
bool frame_pin (void * address, bool user_or_kernal, bool pinval){
  void * paddr;
  struct list_elem iterator;
  struct frame * frame_ptr;
  
  if (!user_or_kernal){//user_or_kernal being false means a virtual address was passed, physical address otherwise
    paddr = pagedir_get_page(thread_current()->pagedir, pg_round_down(vaddr));
  } else {
    paddr = address;
  }
  
  lock_acquire(&frame_lock);
  
  iterator = list_begin(&frame_list);
  while (iterator != list_end(&frame_list)){
    frame_ptr = list_entry(iterator, struct frame, elem);
    if (frame_ptr->paddr == paddr){
      break;
    }
   iterator = list_next(iterator);
  }
  lock_release(&frame_lock);
  if (frame_ptr == NULL){
    return false;
  } 
  frame_ptr->pin = pinval;
  return true;
 }
