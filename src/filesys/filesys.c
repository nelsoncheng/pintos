#include "filesys/filesys.h"

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
  char* fn = filesys_parse_filename(name);
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
  char *fn = filesys_parse_filename(name);

  if (dir != NULL){
     if((dir_root(dir) && strlen(fn) == 0)){
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
               if (!dir_parent(dir, &inode)){
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
  free(fn);
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
  char *fn = filesys_parse_filename(name);
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
   struct dir* dir = filesys_parse_dir(name);
   char* file_name = filesys_parse_filename(name);
   struct inode *inode;
   
   if(dir){
      switch(filesys_check_path_special_char(dir)){// nc
         case CURR_DIRECTORY:
            thread_current()->cwd = dir;
            free(file_name);
            return true;
         case PARENT_DIRECTORY:
            if (!dir_parent(dir, &inode)){
               free(file_name);
               return false;
            }
            break;
         default:
            dir_lookup (dir, file_name, &inode);
            break;
      }
      if (dir_root(dir) && strlen(file_name) == 0){
         thread_current()->cwd = dir;
         free(file_name);
         return true;
      }
   }
   
   dir_close (dir);
   free(file_name);
   
   
   if (dir = dir_open (inode)){
      dir_close(thread_current()->cwd);
      thread_current()->cwd = dir;
      return true;
   }
  return false;
}

struct dir * 
filesys_parse_dir(const char *direc)
{
   
   struct dir *dir;
   struct inode *inode;
   int len = strlen(direc) + 1;
   char buf [len];
   memcpy(buf, direc, len);
   char *ptr, *token_temp, *token = strtok_r(buf, "/", &ptr);
   
   dir = (buf[0] == '/' || !thread_current()->cwd) ? dir_open_root() : dir_reopen(thread_current()->cwd);
   
   token_temp = token ? strtok_r(NULL, "/", &ptr) : NULL;
   
   while(token_temp){
      switch(filesys_check_path_special_char(token)){
         case CURR_DIRECTORY:
         case PARENT_DIRECTORY:
            if(!dir_parent(dir, &inode) || (!dir_lookup(dir, token, &inode)))
               return NULL;
            if (inode_isdir(inode)){
               dir_close(dir);
               dir = dir_open(inode);
            }
            else{
               inode_close(inode);
            }
            break;
         default:
            break;
      }
      token = token_temp;
      token_temp = strtok_r(NULL, "/", &ptr);
   }
   return dir;
}

char *
filesys_parse_filename (const char *dir)
{
   char *token, *save_ptr, *file_name;
   int len = strlen(dir)+1;
   char buf[len];
   memcpy(buf, dir, len);
   
   for(token = strtok_r(buf, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr));
   
   file_name = malloc(strlen(token)+1);
   memcpy(file_name, token, strlen(token)+1);
   return file_name;
   // remember to free file_name per call
}
