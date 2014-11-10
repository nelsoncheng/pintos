#include "vm/frame.h"

void frame_init(){
  list_init(&frame_list);
  lock_init(&frame_lock);
}

void * frame_get(void * vpage, bool zero_page){
  void * kpage = palloc_get_page ( PAL_USER | (zero ? PAL_ZERO : 0) );
  
  if(kpage == NULL) {
    lock_acquire(&frame_lock);
    //TODO: evict a page
    lock_release(&frame_lock);
    return kpage;
  }
  
  struct frame * new_frame = (struct frame*) malloc (sizeof (struct frame));
  new_frame->paddr = kpage
  new_frame->upage = vpage
  new_frame->owner = thread_current();
  new_frame->pin = false;
  lock_acquire(&frame_lock);
  list_push_back(&frame_list, new_frame->elem);
  lock_release(&frame_lock);
  
  return kpage;
}
