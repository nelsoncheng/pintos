#include "vm/swap.h"


void swa
void swap_free_element(struct swap_member *element){
  lock_acquire(&block_lock);
  bitmap_set_multiple(free_list, element->address, page_to_sector_ratio, false);
  lock_release(&block_lock);
}

void swap_init(){
    swap_block = block_get_role(BLOCK_SWAP);
    size = block_size(swap_block);
    page_to_sector_ratio = PGSIZE / BLOCK_SECTOR_SIZE;
    free_list = bitmap_create(size);
    lock_init(&block_lock);
}
