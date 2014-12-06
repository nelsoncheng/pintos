#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* defines the capacities of the various types of inodes */
#define INODE_SIZE 120
#define INODE_INDIRECT_SIZE 128
#define INODE_INDIRECT_2_SIZE 64
#define COMBINED 248 //INODE_SIZE + INODE_INDIRECT_SIZE
#define INODE_SECOND_LEVEL_CAPACITY 8192
#define MAX_SECTORS 16632 //Max number of sectors any file can have


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
 {
   off_t length;                                  /* File size in bytes. */
   unsigned magic;                                /* Magic number. */
 
   block_sector_t blocks[INODE_SIZE];             /* Location of actual data blocks on disk */
   block_sector_t indirect1_block;                /* Pointer to first indirect inode on disk */
   block_sector_t indirect2_block;                /* Pointer to second indirect inode on disk */
   block_sector_t indirect2_2_block;
   struct inode_disk_indirect * indirect1;        /* Pointer to inode of pointers to device blocks */
   struct inode_disk_double_indirect * indirect2; /* Pointer to inode of pointers to inodes of pointers to device blocks */
   struct inode_disk_double_indirect * indirect2_2;
 
 };
  
struct inode_disk_indirect
{
   block_sector_t blocks[INODE_INDIRECT_SIZE];     /* Location of actual data blocks on disk */
};

struct inode_disk_double_indirect
{
   block_sector_t inode_blocks[INODE_INDIRECT_2_SIZE];     /* pointers to inode_disk_indirect on disk */
   struct inode_disk_indirect * table_array[INODE_INDIRECT_2_SIZE]; /* pointers to inode_disk_indirect while in memory*/
};
   
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    //bool isdir;
    //block_sector_t parent;
    
    struct semaphore sema;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  
  if (pos < inode->data.length){
    int sector = pos / 512;
    if (sector <= INODE_SIZE){
	   //PANIC ("%d\n", inode->data.blocks[sector]);
       return inode->data.blocks[sector];

    } else if (sector > INODE_SIZE && sector <= COMBINED) {
       return inode->data.indirect1->blocks[sector - INODE_SIZE];

    } else if (sector > COMBINED && sector <= INODE_SECOND_LEVEL_CAPACITY + COMBINED) {
       int first_level_index = (sector - COMBINED) / INODE_INDIRECT_SIZE;
       int second_level_index = (sector - COMBINED) % INODE_INDIRECT_SIZE;
       return inode->data.indirect2->table_array[first_level_index]->blocks[second_level_index];

    } else if ((sector > INODE_SECOND_LEVEL_CAPACITY + COMBINED) && sector < MAX_SECTORS){
       int first_level_index = (sector - COMBINED - INODE_SECOND_LEVEL_CAPACITY) / INODE_INDIRECT_SIZE;
       int second_level_index = (sector - COMBINED - INODE_SECOND_LEVEL_CAPACITY) % INODE_INDIRECT_SIZE;
       return inode->data.indirect2_2->table_array[first_level_index]->blocks[second_level_index];

    } else {
       return -1;
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}



/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      if (sectors > MAX_SECTORS || sectors > free_map_count_free()){
         return false;
      }	  
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      block_sector_t indirect_sector;
      block_sector_t * sector_pos_array = (block_sector_t *) calloc (1, sectors * sizeof (block_sector_t));

      if (free_map_allocate_discontinuous (sectors, sector_pos_array)) 
        {
			
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i, j;
              
              for (i = 0; i < sectors; i++) {
                block_write (fs_device, sector_pos_array[i], zeros);
              }
              
              if (sectors <= INODE_SIZE){		
				 	
				 for (i = 0; i < sectors; i++){
				 	disk_inode->blocks[i] = sector_pos_array[i];
				 }			
		 		 block_write (fs_device, sector, disk_inode);
                 free (disk_inode);	
				 free (sector_pos_array);	 
              } else if (sectors > INODE_SIZE && sectors <= COMBINED){
                 disk_inode->indirect1 = (struct inode_disk_indirect*) calloc (1, sizeof (struct inode_disk_indirect));
                 for (i = 0; i < INODE_SIZE; i++){
				 	disk_inode->blocks[i] = sector_pos_array[i];
				 }
				 for (i = 0; i < sectors - INODE_SIZE; i++){
				 	disk_inode->indirect1->blocks[i] = sector_pos_array[i + (INODE_SIZE)];
				 }	
                 
                 free_map_allocate (1, &indirect_sector);
                 disk_inode->indirect1_block = indirect_sector;
		 block_write (fs_device, sector, disk_inode);
                 block_write (fs_device, indirect_sector, disk_inode->indirect1);
                 free (disk_inode->indirect1);
                 free (disk_inode);
              } else if (sectors > COMBINED) {
                disk_inode->indirect1 = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
                disk_inode->indirect2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
                for (i = 0; i < INODE_SIZE; i++){
				 	disk_inode->blocks[i] = sector_pos_array[i];
				 }
				 for (i = 0; i < INODE_INDIRECT_SIZE; i++){
				 	disk_inode->indirect1->blocks[i] = sector_pos_array[i + (INODE_SIZE)];
				 }	
                 
                free_map_allocate (1, &indirect_sector);
                disk_inode->indirect1_block = indirect_sector;
                free_map_allocate (1, &indirect_sector);
                disk_inode->indirect2_block = indirect_sector;
                 
		block_write (fs_device, sector, disk_inode);
                block_write (fs_device, indirect_sector, disk_inode->indirect1);
                 
                int i;
                int first_level_index = sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) ? 
                (INODE_SECOND_LEVEL_CAPACITY / INODE_INDIRECT_SIZE) : (sectors - COMBINED) / INODE_INDIRECT_SIZE;
                int sectors_remaining = sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) ? 
                INODE_SECOND_LEVEL_CAPACITY : (sectors - COMBINED);
                for (i = 0; i < first_level_index; i++){
                    disk_inode->indirect2->table_array[i] = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
                    free_map_allocate (1, &indirect_sector);
                    disk_inode->indirect2->inode_blocks[i] = indirect_sector;
                    if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							disk_inode->indirect2->table_array[i]->blocks[j] = sector_pos_array[j + COMBINED + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							disk_inode->indirect2->table_array[i]->blocks[j] = sector_pos_array[j + COMBINED + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
                    block_write(fs_device, indirect_sector, disk_inode->indirect2->table_array[i]);
                    free (disk_inode->indirect2->table_array[i]);
                    sectors_remaining -= INODE_INDIRECT_SIZE;
                }
                  
                block_write (fs_device, disk_inode->indirect2_block, disk_inode->indirect2);
                  
                if (sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED)){
                    disk_inode->indirect2_2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
                    free_map_allocate (1, &indirect_sector);
                    disk_inode->indirect2_2_block = indirect_sector;
                    first_level_index = (sectors - INODE_SECOND_LEVEL_CAPACITY - COMBINED) / INODE_INDIRECT_SIZE;
                    sectors_remaining = sectors - INODE_SECOND_LEVEL_CAPACITY - COMBINED;
                    
                    for (i = 0; i < first_level_index; i++){
                        disk_inode->indirect2_2->table_array[i] = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
                        free_map_allocate (1, &indirect_sector);
                        disk_inode->indirect2_2->inode_blocks[i] = indirect_sector;
                        if (sectors_remaining < INODE_INDIRECT_SIZE){
							for (j = 0; j < sectors_remaining; j++){
								disk_inode->indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + COMBINED + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   		}
                        } else {
							for (j = 0; j < INODE_INDIRECT_SIZE; j++){
								disk_inode->indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + COMBINED + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   		}  
                        }
                        block_write(fs_device, indirect_sector, disk_inode->indirect2->table_array[i]);
                        free (disk_inode->indirect2_2->table_array[i]);
                        sectors_remaining -= INODE_INDIRECT_SIZE;
                    }
                    block_write(fs_device, disk_inode->indirect2_2_block, disk_inode->indirect2_2);
                    free (disk_inode->indirect2_2);
                }
                free (disk_inode->indirect1);
                free (disk_inode->indirect2);
                free (disk_inode);
               }
            }

          success = true; 
        } 
      
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = calloc (1, sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  sema_init(&inode->sema, 1);

  block_read (fs_device, inode->sector, &inode->data);
  int size_of_inode = bytes_to_sectors(inode->data.length);

  if (size_of_inode > INODE_SIZE){
	//PANIC("sector size greater than INODE_SIZE\n");
     inode->data.indirect1 = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
     block_read(fs_device, inode->data.indirect1_block, inode->data.indirect1);
  }
  if (size_of_inode > COMBINED){
	//PANIC("sector size greater than INODE_SIZE\n");
     inode->data.indirect2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
     block_read(fs_device, inode->data.indirect2_block, inode->data.indirect2);
     int i, first_level_index;
     
     first_level_index = size_of_inode > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) ? 
      INODE_SECOND_LEVEL_CAPACITY / INODE_INDIRECT_SIZE : (size_of_inode - COMBINED ) / INODE_INDIRECT_SIZE;
     for (i = 0; i < first_level_index; i++){
        inode->data.indirect2->table_array[i] = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
        block_read(fs_device, inode->data.indirect2->inode_blocks[i], &inode->data.indirect2->table_array[i]);
     }
  }
  if (size_of_inode > INODE_SECOND_LEVEL_CAPACITY){
     inode->data.indirect2_2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
     block_read(fs_device, inode->data.indirect2_2_block, &inode->data.indirect2_2);
     int i, first_level_index;
     
     first_level_index = (size_of_inode - COMBINED - INODE_SECOND_LEVEL_CAPACITY) / INODE_INDIRECT_SIZE;
     for (i = 0; i < first_level_index; i++){
        inode->data.indirect2_2->table_array[i] = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
        block_read(fs_device, inode->data.indirect2_2->inode_blocks[i], &inode->data.indirect2_2->table_array[i]);
     }
  }

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 	  block_write (fs_device, inode->sector, &inode->data);
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
         free_map_release (inode->sector, 1);      
         int sectors, i, j;
         sectors = bytes_to_sectors(inode->data.length);
         if (sectors <= INODE_SIZE){
            for (i = 0; i < sectors; i++){
               free_map_release (inode->data.blocks[i], 1); 
            }
         } else if (sectors > INODE_SIZE && sectors <= COMBINED) {
			//PANIC("sector size greater than INODE_SIZE\n");
            block_write (fs_device, inode->data.indirect1_block, inode->data.indirect1);
            for (i = 0; i < INODE_SIZE; i++){
               free_map_release (inode->data.blocks[i], 1); 
            }
            for (i = 0; i < (sectors - INODE_SIZE); i++){
               free_map_release (inode->data.indirect1->blocks[i], 1); 
            }
            free (inode->data.indirect1);
          } else if (sectors > COMBINED) {
             //PANIC("sector size greater than INODE_SIZE\n");
            //write the default inode and first level inode to disk
            block_write (fs_device, inode->data.indirect1_block, inode->data.indirect1);
            block_write (fs_device, inode->data.indirect2_block, inode->data.indirect2);
            for (i = 0; i < INODE_SIZE; i++){
               free_map_release (inode->data.blocks[i], 1); 
            }
            for (i = 0; i < INODE_INDIRECT_SIZE; i++){
               free_map_release (inode->data.indirect1->blocks[i], 1); 
            }
            
            //write second level inode and possible second second level inode to disk
            int index = sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) ? 
            (INODE_SECOND_LEVEL_CAPACITY / INODE_INDIRECT_SIZE) : (sectors - COMBINED) / INODE_INDIRECT_SIZE;
            int sectors_remaining = sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) ? 
            INODE_SECOND_LEVEL_CAPACITY : (sectors - COMBINED);
            for (i = 0; i < index; i++){
               block_write (fs_device, inode->data.indirect2->inode_blocks[i], inode->data.indirect2->table_array[i]);
               if (sectors_remaining < INODE_INDIRECT_SIZE){
                 for (j = 0; j < sectors_remaining; j++) {
                     free_map_release (inode->data.indirect2->table_array[i]->blocks[j], 1);
                 }
                  free (inode->data.indirect2->table_array[i]);
               } else {
                  for (j = 0; j < INODE_INDIRECT_SIZE; j++){
                     free_map_release (inode->data.indirect2->table_array[i]->blocks[j], 1);
                  }
                  free (inode->data.indirect2->table_array[i]);
               }
               sectors_remaining -= INODE_INDIRECT_SIZE;
            }
            
            //check of the second second level inode was used
            if (sectors > (INODE_SECOND_LEVEL_CAPACITY + COMBINED)){
               index = (sectors - INODE_SECOND_LEVEL_CAPACITY - COMBINED) / INODE_INDIRECT_SIZE;
               sectors_remaining = sectors - INODE_SECOND_LEVEL_CAPACITY - COMBINED;
               for (i = 0; i < index; i++){
                  block_write (fs_device, inode->data.indirect2_2->inode_blocks[i], inode->data.indirect2_2->table_array[i]);
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
                     for (j = 0; j < sectors_remaining; j++) {
                        free_map_release (inode->data.indirect2_2->table_array[i]->blocks[j], 1);
                     }
                     free (inode->data.indirect2_2->table_array[i]);
                  } else {
                     for (j = 0; j < INODE_INDIRECT_SIZE; j++){
                        free_map_release (inode->data.indirect2->table_array[i]->blocks[j], 1);
                     }
                     free (inode->data.indirect2_2->table_array[i]);
                  }
                  sectors_remaining -= INODE_INDIRECT_SIZE;
               }
               free (inode->data.indirect2_2);
            }
            free (inode->data.indirect2);
            free (inode->data.indirect1);
          }
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  int original_size, original_offset;

  original_size = size;
  original_offset = offset;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
	//PANIC ("%d\n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
 
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = calloc (1, BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  block_sector_t indirect_sector;

  if (inode->deny_write_cnt)
    return 0;
  int temp = size;
  if ((size + offset) > inode->data.length){
     //grow the file
	  sema_down(&inode->sema);
      int sectors = bytes_to_sectors(size + offset - inode->data.length);
      int sectors_so_far = inode->data.length / 512;
      int total_sectors = sectors + sectors_so_far;
     
      block_sector_t * sector_pos_array = (block_sector_t *) calloc (1, sectors * sizeof(block_sector_t));
      if (!free_map_allocate_discontinuous(sectors, sector_pos_array)){
         //filesystem cannot accomodate this growth, write nothing
         return 0;
      }
     
      static char zeros[BLOCK_SECTOR_SIZE];
      int i, first_level, second_level, third_level, fourth_level, j, sectors_remaining, sectors_to_write;
      for (i = 0; i < sectors; i++) {
         block_write (fs_device, sector_pos_array[i], zeros);
      }

      int overflow = inode_overflow (sectors_so_far, total_sectors); 
      if (sectors_so_far <= INODE_SIZE){
         switch (overflow){
            case 0:
			   sectors_to_write = total_sectors <= INODE_SIZE ? sectors : INODE_SIZE - sectors_so_far;
          	   for (i = 0; i < sectors_to_write; i++){
					inode->data.blocks[sectors_so_far + i] = sector_pos_array[i];
	  			}
               if (total_sectors <= INODE_SIZE) break;
            case 1:
			   sectors_to_write = total_sectors <= COMBINED ? sectors - INODE_SIZE - sectors_so_far : INODE_INDIRECT_SIZE;
               inode->data.indirect1 = (struct inode_disk_indirect *) calloc (1, sizeof (struct inode_disk_indirect));
               free_map_allocate (1, &indirect_sector);
               inode->data.indirect1_block = indirect_sector;
               first_level = INODE_SIZE - sectors_so_far;
				for (i = 0; i < sectors - first_level; i++){
					inode->data.indirect1->blocks[i] = sector_pos_array[i + first_level];
				}
               if (total_sectors <= COMBINED) break;
            case 2:
			   sectors_to_write = total_sectors <= COMBINED ? sectors - INODE_SIZE - sectors_so_far : INODE_INDIRECT_SIZE;
               inode->data.indirect2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
               free_map_allocate (1, &indirect_sector);
               inode->data.indirect2_block = indirect_sector;
               first_level = INODE_SIZE - sectors_so_far;
               second_level = INODE_INDIRECT_SIZE;
               third_level = sectors - COMBINED;
				sectors_remaining = third_level;
               for (i = 0; i < third_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							inode->data.indirect2->table_array[i]->blocks[j] = sector_pos_array[j + INODE_INDIRECT_SIZE + first_level + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							inode->data.indirect2->table_array[i]->blocks[j] = sector_pos_array[j + INODE_INDIRECT_SIZE + first_level + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				if (total_sectors <= COMBINED + INODE_SECOND_LEVEL_CAPACITY) break;
			case 3:
				inode->data.indirect2_2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
               free_map_allocate (1, &indirect_sector);
               inode->data.indirect2_2_block = indirect_sector;
               first_level = INODE_SIZE - sectors_so_far;
               second_level = INODE_INDIRECT_SIZE;
               third_level = COMBINED;
 				fourth_level = sectors - COMBINED - INODE_SECOND_LEVEL_CAPACITY;
				sectors_remaining = fourth_level;
               for (i = 0; i < fourth_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + INODE_INDIRECT_SIZE + first_level + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + INODE_INDIRECT_SIZE + first_level + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				break;
         }
      } else if (sectors_so_far > INODE_SIZE && sectors_so_far <= COMBINED){
         switch (overflow){
            case 0:
               first_level = COMBINED - sectors_so_far;
				for (i = 0; i < sectors - first_level; i++){
					inode->data.indirect1->blocks[sectors_so_far + i] = sector_pos_array[i + first_level];
				}
               if (total_sectors <= COMBINED) break;
            case 1:
               inode->data.indirect2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
               free_map_allocate (1, &indirect_sector);
               inode->data.indirect2_block = indirect_sector;
               first_level = COMBINED - sectors_so_far;
               third_level = sectors - COMBINED;
				sectors_remaining = third_level;
               for (i = 0; i < third_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							inode->data.indirect2->table_array[i]->blocks[j] = sector_pos_array[j + first_level + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							inode->data.indirect2->table_array[i]->blocks[j] = sector_pos_array[j + first_level + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				if (total_sectors <= COMBINED + INODE_SECOND_LEVEL_CAPACITY) break;
			case 2:
				inode->data.indirect2_2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
               free_map_allocate (1, &indirect_sector);
               inode->data.indirect2_2_block = indirect_sector;
               first_level = INODE_SIZE - sectors_so_far;
 				fourth_level = sectors - COMBINED - INODE_SECOND_LEVEL_CAPACITY;
				sectors_remaining = fourth_level;
               for (i = 0; i < fourth_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + first_level + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j  + first_level + INODE_SECOND_LEVEL_CAPACITY + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				break;
             }
      } else if (sectors_so_far > COMBINED && sectors_so_far <= INODE_SECOND_LEVEL_CAPACITY + COMBINED) {
			first_level = COMBINED + INODE_SECOND_LEVEL_CAPACITY - sectors_so_far;
       		switch (overflow){
            case 0:                              
				sectors_remaining = first_level;
				int i_offset = (sectors_so_far - COMBINED) / 512;
				int j_offset = (sectors_so_far - COMBINED) % 512;
               for (i = 0; i < first_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							if (i == 0) inode->data.indirect2->table_array[i + i_offset]->blocks[j + j_offset] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
							else inode->data.indirect2->table_array[i + i_offset]->blocks[j] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							if (i == 0) inode->data.indirect2->table_array[i + i_offset]->blocks[j + j_offset] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
							else inode->data.indirect2->table_array[i + i_offset]->blocks[j + j_offset] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				if (total_sectors <= COMBINED + INODE_SECOND_LEVEL_CAPACITY) break;
			case 1:
				inode->data.indirect2_2 = (struct inode_disk_double_indirect *) calloc (1, sizeof (struct inode_disk_double_indirect));
                free_map_allocate (1, &indirect_sector);
                inode->data.indirect2_2_block = indirect_sector;
 				fourth_level = sectors - COMBINED - INODE_SECOND_LEVEL_CAPACITY;
				sectors_remaining = fourth_level;
               for (i = 0; i < fourth_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + first_level + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							inode->data.indirect2_2->table_array[i]->blocks[j] = sector_pos_array[j + first_level + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
               }
				break;
             }
      } else {
				int i_offset = (sectors_so_far - COMBINED - INODE_SECOND_LEVEL_CAPACITY) / 512;
				int j_offset = (sectors_so_far - COMBINED - INODE_SECOND_LEVEL_CAPACITY) % 512;
 				first_level = sectors;
				sectors_remaining = first_level;
               for (i = 0; i < first_level/INODE_INDIRECT_SIZE; i++){
                  if (sectors_remaining < INODE_INDIRECT_SIZE){
					   for (j = 0; j < sectors_remaining; j++){
							if (i == 0) inode->data.indirect2_2->table_array[i + i_offset]->blocks[j + j_offset] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
							else inode->data.indirect2_2->table_array[i + i_offset]->blocks[j] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
					   }
                    } else {
						for (j = 0; j < INODE_INDIRECT_SIZE; j++){
							if (i == 0) inode->data.indirect2_2->table_array[i + i_offset]->blocks[j + j_offset] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
							else inode->data.indirect2_2->table_array[i + i_offset]->blocks[j] = sector_pos_array[j + (i* INODE_INDIRECT_SIZE)];
					   }                      
                    }
					sectors_remaining -= INODE_INDIRECT_SIZE;
              }            
			}	  
	  sema_up(&inode->sema);
	  inode->data.length = size + offset;
	}
   while (size > 0) 
   {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;
		
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
			
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = calloc (1, BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) {
            block_read (fs_device, sector_idx, bounce);
			
			}
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  //PANIC ( "bytes written %d %d\n", bytes_written, temp);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

static int inode_overflow (int sectors_so_far, int total_sectors){
	if (sectors_so_far <= INODE_SIZE){
		if (total_sectors <= INODE_SIZE) return 0;
		if (total_sectors <= COMBINED) return 1;
		if (total_sectors <= (COMBINED + INODE_SECOND_LEVEL_CAPACITY)) return 2;
		if (total_sectors <= MAX_SECTORS) return 3;
	} else if (sectors_so_far > INODE_SIZE && sectors_so_far <= COMBINED){
		if (total_sectors <= COMBINED) return 0;
		if (total_sectors <= (COMBINED + INODE_SECOND_LEVEL_CAPACITY)) return 1;
		if (total_sectors <= MAX_SECTORS) return 2;
	} else if (sectors_so_far > COMBINED && sectors_so_far <= (INODE_SECOND_LEVEL_CAPACITY + COMBINED)){
		if (total_sectors <= (COMBINED + INODE_SECOND_LEVEL_CAPACITY)) return 0;
		if (total_sectors <= MAX_SECTORS) return 1;
	} else if (sectors_so_far > (INODE_SECOND_LEVEL_CAPACITY + COMBINED) && sectors_so_far < MAX_SECTORS){
		if (total_sectors <= MAX_SECTORS) return 0;
	}
	return -1;
}
