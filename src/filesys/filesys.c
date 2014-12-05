#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  char* fn = filesys_parse_file_name(name);
  bool success;
  if (filesys_check_path_special_char (fn) != NONE){
      success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  }
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(fn);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  char *fn = filesys_parse_file_name(name);

  if (dir != NULL){
     if((dir_is_root(dir) && strlen(file_name) == 0)){
         thread_current()->cwd = dir;
         free(fn);
         return true;
     }
     else{
        switch(filesys_check_path_special_char(fn)){
            case CURR_DIRECTORY:
               thread_current()->cwd = dir;
               free(fn);
               return (struct file*) dir;
            case PARENT_DIRECTORY:
               if (!dir_get_parent(dir, &inode)){
                  free(fn);
                  return NULL;
               }
               break;
            default:
               dir_lookup (dir, name, &inode);
               break;
        }
     }
  }
    
  dir_close (dir);
  free(filename);
  if(dir = dir_open(inode)){
      dir_close(thread_current()->cwd);
      thread_current()->cwd = dir;
      return true;
  }
  return false;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = filesys_parse_dir (name);
  char *fn = filesys_parse_file_name(name);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 
  free(fn);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

directory_token_t
filesys_check_path_special_char (const char *path)
{
   if(!strcmp(path, "."))
      return CURR_DIRECTORY;
   else if (!strcmp(path, ".."))
      return PARENT_DIRECTORY;
   return NONE;
}

bool 
filesys_chdir (const char *name)
{
   //TODO: do this
}

struct dir * 
filesys_parse_dir(const char *direc)
{
   char *ptr, *token1, *token = strtok_r(buf, "/", &ptr);
   struct dir *dir;
   int len = strlen(path) + 1;
   char buf [len];
   memcpy(buf, direct, len);
   
   //TODO: do this
}

char *
filesys_parse_filename (const char *dir)
{
   char *token, *save_ptr, *file_name;
   int length = strlen(dir)+1;
   char buf[len];
   memcpy(buf, path, len);
   
   for(token = strtok_r(buf, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr));
   
   file_name = malloc(strlen(token)+1);
   memcpy(file_name, token, strlen(prev_token)+1);
   return file_name;
   // remember to free file_name per call
}
