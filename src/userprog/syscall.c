#include "userprog/syscall.h"

#define ARG1 (syscall-4)
#define ARG2 (syscall-8)
#define ARG3 (syscall-12)

static void syscall_handler (struct intr_frame *);
static void syscall_halt (void);
static void syscall_exit (int status);
static pid_t syscall_exec (const char *cmd_line);
static int syscall_wait (pid_t pid);
static bool syscall_create (const char *file, unsigned initial_size);
static bool syscall_remove (const char *file);
static int syscall_open (const char *file);
static int syscall_filesize (int fd);
static int syscall_read (int fd, void *buffer, unsigned size);
static int syscall_write (int fd, const void *buffer, unsigned size);
static void syscall_seek (int fd, unsigned position);
static struct fd_elem * find_fd_elem (int fd);
static int is_valid_pointer (void *p);
static void syscall_seek (int fd, unsigned position);
static unsigned syscall_tell (int fd);
static void syscall_close (int fd);

struct list file_list;
struct list exit_status_list;
struct lock file_lock;

int fd_counter;


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init (&file_list);
  list_init (&exit_status_list);
  lock_init (&file_lock);
  fd_counter = 2;
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *syscall;
  int status;
  
  syscall = f->esp;
  // check if we decrement first, decrement syscalls by 8, 12, 16
  // syscall = syscall - 4;
  printf("syscall: %d\n", (int)syscall);
  switch (*syscall) {
	case SYS_WRITE: 
		status = syscall_write((int)* ARG1, (void*)* ARG2, (unsigned)* ARG3);
		break;
	case SYS_EXIT:
		syscall_exit((int)* ARG1);
		break;
    case SYS_HALT:
        syscall_halt();
		break;
	case SYS_EXEC:
        status = syscall_exec((char*)* ARG1);
		break;
	case SYS_WAIT:
        status = syscall_wait((pid_t)* ARG1);
		break;
	case SYS_CREATE:
        status = syscall_create((char*)* ARG1, (unsigned)* ARG2);
		break;
	case SYS_REMOVE:
        status = syscall_remove((char*)* ARG1);
		break;
	case SYS_OPEN:
        status = syscall_open((char*)* ARG1);
		break;
	case SYS_FILESIZE:
        status = syscall_filesize((int)* ARG1);
		break;
	case SYS_READ:
        status = syscall_read((int)* ARG1, (void*)* ARG2, (unsigned)* ARG3);
		break;
	case SYS_SEEK:
        syscall_seek((int)* ARG1, (unsigned)* ARG2);
		break;
	case SYS_TELL:
        status = syscall_tell((int)* ARG1);
		break;
	case SYS_CLOSE:
        syscall_close((int)* ARG1);
		break;
	default:
		status = -1;
  }

  if (status != -1) {
    f->eax = status;
  }
}

static void syscall_halt (void){
	shutdown_power_off();
}

static void syscall_exit (int status){
	struct status_elem *status_e;
	/*struct list_elem *e;
	curr_thread = thread_current ();
	while (!list_empty (&curr_thread->files)){
		e = list_begin (&curr_thread->files);
		syscall_close (list_entry (e, struct fd_elem, thread_elem)->fd);
	}*/
	
	//TODO: condition for if parent is alive
	status_e = malloc(sizeof(struct status_elem));
	status_e->pid = thread_current()->tid;
        status_e->status = status;
	list_push_back (&exit_status_list, &status_e->elem);
	//sema_up(parent_sema);
	//might need to use write instead of print
	printf("%s: exit(%d)\n", (char *)(thread_current()->file_name), status);
	thread_exit();
}

static pid_t syscall_exec (const char *cmd_line){
	pid_t pid;
	struct child_elem *child_e;
	
    //check for bad pointer
	if (!is_valid_pointer(cmd_line))
		syscall_exit(-1);
	pid = process_execute (cmd_line);
	if (pid != -1){
		//add this PID to the current thread's children list
		child_e = malloc(sizeof(struct child_elem));
		child_e->pid = pid;
		list_push_back (&thread_current()->children, &child_e->elem);
	}
	return pid;
}

static int syscall_wait (pid_t pid){
  struct thread *curr_thread = thread_current();
  struct list_elem *e;
  struct child_elem *child_e;
  struct status_elem *status_e;
  int child_found = 0;
  
  if(pid< 0){
  	return -1;
  }
  // check if the pid passed is actually a child of this thread
  curr_thread = thread_current ();
  e = list_begin(&curr_thread->children);
  while (e != list_end (&curr_thread->children)){
	child_e = list_entry(e, struct child_elem, elem);
	if (child_e->pid == pid){
		child_found++;
		list_remove(e); //remove the child from the list so the parent can't wait twice
		break;
	}
	e = list_next(e);
  }
  if (child_found == 0){
  	return -1;
  }
   e = list_begin(&exit_status_list);
   
  // look through list of exited threads to see if child is already dead
  while (e != list_end (&exit_status_list)){
	status_e = list_entry(e, struct status_elem, elem);
	if (status_e->pid == pid){
		list_remove(e);
		return status_e->status;
	}
	e = list_next(e);
  }
}

static bool syscall_create (const char *file, unsigned initial_size){
	//return !file ? syscall_exit(-1) : filesys_create (file, initial_size);
	if (!is_valid_pointer(file)){
		syscall_exit(-1);
		return false;
	}
	else
		filesys_create (file, initial_size);
}

static bool syscall_remove (const char *file){
	if (!file)
		return false;
	else if (!is_user_vaddr (file)){
		printf("invalid user virtual address");
		syscall_exit (-1); 
		return false;
	}
	else
		return filesys_remove (file);
}

static int syscall_open (const char *file){
	struct file *process_file;
	struct fd_elem *fde;
	int status;

	status = -1;
	if (!file)
		return -1;
	if (!is_user_vaddr (file)){
		syscall_exit (-1);
		return -1;
	}
		
	process_file = filesys_open (file);
	if (!process_file)
		return status;

	fde = (struct fd_elem *)malloc (sizeof (struct fd_elem));
	if (!fde){
		printf("Not enough memory\n");
		file_close (process_file);
		return status;
	}
	
	/* allocate fde an ID, put fde in file_list, put fde in the current thread's file_list */
	fde->file = process_file; 
	//TODO: write allocator for fd
	fde->fd = fd_counter++;
	list_push_back (&file_list, &fde->elem);
	list_push_back (&thread_current ()->files, &fde->thread_elem);
	status = fde->fd;
	return status;
}

static int syscall_filesize (int fd){
	struct file *process_file;

	//TODO: find file method
	process_file = find_fd_elem (fd)->file;
	return !process_file ? -1 : file_length(process_file);
}

static int syscall_read (int fd, void *buffer, unsigned size){
	struct file * process_file;
	unsigned i;
	int status;

	status = -1;
	//TODO: Add file_lock to thread
	lock_acquire (&file_lock);

	if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
		lock_release (&file_lock);
		syscall_exit(-1);
	}
	switch(fd){
		case STDIN_FILENO:
			for (i = 0; i != size; ++i)
				*(uint8_t *)(buffer + i) = input_getc ();
			status = size;
			lock_release (&file_lock);
			return status;
		case STDOUT_FILENO:
			lock_release (&file_lock);
			return status;
		default:
			process_file = find_fd_elem (fd)->file;
			if (!process_file){
				lock_release (&file_lock);
				return status;
			}
			status = file_read (process_file, buffer, size);
			lock_release (&file_lock);
			return status;
	}
}

static int syscall_write (int fd, const void *buffer, unsigned size){

	struct file * process_file;
	unsigned i;
	int status;
	
	for (i = 0; i < size; ++i){
		if (!is_valid_pointer(buffer + i)){
			syscall_exit(-1);
		}
	}
	lock_acquire (&file_lock);
	status = -1;
	switch(fd){
		case STDIN_FILENO:
			for (i = 0; i != size; ++i)
				*(uint8_t *)(buffer + i) = input_getc ();
			status = size;
			lock_release (&file_lock);
			return status;
		case STDOUT_FILENO:
			putbuf (buffer, size);
			lock_release (&file_lock);
			return status;
		default:
			process_file = find_fd_elem (fd)->file;
			if (!process_file){
				lock_release (&file_lock);
				return status;
			}
			status = file_write (process_file, buffer, size);
			lock_release (&file_lock);
			return status;
	}

	return size;
}

static void syscall_seek (int fd, unsigned position){
	struct file *process_file;
	
	//TODO: again, find file method
	process_file = find_fd_elem (fd)->file;
	if (!process_file)
		syscall_exit(-1);
	file_seek (process_file, (off_t)position);
}

static unsigned syscall_tell (int fd){
	struct file *process_file;
	
	//TODO: again, find file method
	process_file = find_fd_elem (fd)->file;
	if (!process_file)
		return -1;
	return file_tell (process_file);
}
static void syscall_close (int fd){
	struct fd_elem *fde;
  
	fde = find_fd_elem (fd);

	if (!fde) 
		syscall_exit (-1);

	file_close (fde->file);
	list_remove (&fde->elem);
	list_remove (&fde->thread_elem);
	free (fde);
	return syscall_exit(1);
}

////////////////////////////////////////////////////////////////////////////////////////
//Support methods
////////////////////////////////////////////////////////////////////////////////////////
static struct fd_elem * find_fd_elem (int fd){
	struct fd_elem *curr_elem;
	struct list_elem *e;
	
	e = list_begin(&file_list);
	while (e != list_end (&file_list)){
		curr_elem = list_entry(e, struct fd_elem, elem);
		if (curr_elem->fd == fd)
			return curr_elem;
		e = list_next(e);
	}
	
	return NULL;
}

static int is_valid_pointer (void *p){
	if ((p == NULL ) || is_user_vaddr(p) || (pagedir_get_page(thread_current()->pagedir, p) == NULL))
		return 0;
	return 1;
}
