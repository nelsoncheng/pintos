#include "vm/swap.h"


//Jonathan coding
struct swap_member * swap_insert(struct frame *frame_ptr){
  int i;
  lock_acquire(&block_lock);
  //aqcuire a spot on the free list
  block_sector_t free_spot = bitmap_scan(free_list, 0, page_to_sector_ratio, true);
  if (free_spot == BITMAP_ERROR){
    PANIC ("Swap device is full!");
  }
  bitmap_set_multiple(free_list, free_spot, page_to_sector_ratio, false);
  //write the stuff in the frame to the block
  for ( i = 0; i < page_to_sector_ratio; i++){
    void * paddr_to_write = (frame_ptr->paddr) + (BLOCK_SECTOR_SIZE * i);
    block_write(swap_block, free_spot + i, paddr_to_write);
  }
  lock_release(&block_lock);
  //create a swap_member for the new page were about to create to be able to retrieve information from swap
  struct swap_member * member = malloc(sizeof(struct swap_member));
  member->start_address = free_spot;
  member->frame_ptr = frame_ptr;
  return member;
}

//Nelson coding
void swap_retrieve(struct swap_member * member, void * physical_address){
  lock_acquire(&block_lock);
  int i;
  bitmap_set_multiple(free_list, member->start_address, page_to_sector_ratio, true);
  for (i = 0; i < page_to_sector_ratio; i++){
    block_read(swap_block, member->start_address + i, physical_address + (BLOCK_SECTOR_SIZE * i));
  }
  lock_release(&block_lock);
}

void swap_free_bitmap(struct swap_member *member){
  lock_acquire(&block_lock);
  bitmap_set_multiple(free_list, member->start_address, page_to_sector_ratio, true);
  lock_release(&block_lock);
}

//Jonthan coding
void swap_init(){
    swap_block = block_get_role(BLOCK_SWAP);
    size = block_size(swap_block);
    page_to_sector_ratio = PGSIZE / BLOCK_SECTOR_SIZE;
    free_list = bitmap_create(size);
    bitmap_set_all(free_list, true);
    lock_init(&block_lock);
}
