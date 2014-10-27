#include "userprog/syscall.h"

#define ARG1 (syscall+1)
#define ARG2 (syscall+2)
#define ARG3 (syscall+3)

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

struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *syscall;
  int status;
  
  syscall = f->esp;
  if (!is_valid_pointer(syscall)){
		syscall_exit(-1);
  }
  // check if we decrement first, decrement syscalls by 8, 12, 16
  // syscall = syscall - 4;
  switch (*syscall) {
	case SYS_WRITE: 
		if (!is_valid_pointer(ARG1) || !is_valid_pointer(ARG2) || !is_valid_pointer(ARG3)){
		syscall_exit(-1);
 		}
		status = syscall_write((int)* ARG1, (void*)* ARG2, (unsigned)* ARG3);
		break;
	case SYS_EXIT:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
		syscall_exit((int)* ARG1);
		break;
    case SYS_HALT:
        syscall_halt();
		break;
	case SYS_EXEC:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_exec((char*)* ARG1);
		break;
	case SYS_WAIT:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_wait((pid_t)* ARG1);
		break;
	case SYS_CREATE:
		if (!is_valid_pointer(ARG1) || !is_valid_pointer(ARG2)){
		syscall_exit(-1);
 		}
        status = syscall_create((char*)* ARG1, (unsigned)* ARG2);
		break;
	case SYS_REMOVE:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_remove((char*)* ARG1);
		break;
	case SYS_OPEN:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_open((char*)* ARG1);
		break;
	case SYS_FILESIZE:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_filesize((int)* ARG1);
		break;
	case SYS_READ:
		if (!is_valid_pointer(ARG1) || !is_valid_pointer(ARG2) || !is_valid_pointer(ARG3)){
		syscall_exit(-1);
 		}
        status = syscall_read((int)* ARG1, (void*)* ARG2, (unsigned)* ARG3);
		break;
	case SYS_SEEK:
		if (!is_valid_pointer(ARG1) || !is_valid_pointer(ARG2)){
		syscall_exit(-1);
 		}
        syscall_seek((int)* ARG1, (unsigned)* ARG2);
		break;
	case SYS_TELL:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        status = syscall_tell((int)* ARG1);
		break;
	case SYS_CLOSE:
		if (!is_valid_pointer(ARG1)){
		syscall_exit(-1);
 		}
        syscall_close((int)* ARG1);
		break;
	default:
		status = -1;
  }

  f->eax = status;
  
}

static void syscall_halt (void){
	shutdown_power_off();
}

static void syscall_exit (int status){
	struct list_elem *child_list_elem, *status_list_elem, *all_list_elem;
	struct status_elem *status_e;
	struct child_elem *child_e;
	struct thread *thread_e;
	struct thread *curr_thread;
	int child_pid, parent_thread_alive, test;
	char * name_ptr, save_ptr;

	//printf("in exit!!!!!!!!!!!!!!!!!!!!!\n");
	
	//close all of the current thread's open files
	curr_thread = thread_current();
	struct list_elem *e;
	while (!list_empty (&curr_thread->files)){
		e = list_begin (&curr_thread->files);
		syscall_close (list_entry (e, struct fd_elem, elem)->fd);
	}
	
	child_list_elem = list_begin(&curr_thread->children);
	while (child_list_elem != list_end(&curr_thread->children)){
		child_e = list_entry(child_list_elem, struct child_elem, elem);
		child_pid = child_e->pid;
		
		status_list_elem = list_begin(&curr_thread->exit_status_list);
  		// look through list of exited threads to see if child is already dead, remove its exit status if so
  		while (status_list_elem != list_end (&curr_thread->exit_status_list)){
			status_e = list_entry(status_list_elem, struct status_elem, elem);
			if (status_e->pid == child_pid){
				list_remove(status_list_elem);
				//free(status_e);
			}
			status_list_elem = list_next(status_list_elem);
  		}
		child_list_elem = list_next(child_list_elem);
	}
	
	//check if parent is alive by searching through list of all threads 
	/*
	all_list_elem = list_begin(&all_list);
	while (all_list_elem != list_end(&all_list)){
		thread_e = list_entry(all_list_elem, struct thread, allelem);
		test = (int)(thread_e->tid);
		printf("%d\n", test);
		printf("%d\n", (int) thread_e);
		printf("%d\n", (int) &(thread_e->tid));
		if ((thread_e->tid) == (int)(curr_thread->parent_pid)){
			parent_thread_alive = 1;
			break;
		}
		all_list_elem = list_next(all_list_elem);
	}
	*/
	
	//if (parent_thread_alive){
	if (true){
		//printf("adding to exit status list\n");
		//printf("sizeof struct status_elem: %d\n", sizeof(struct status_elem));
		status_e = malloc(sizeof(struct status_elem));
		status_e->pid = curr_thread->tid;
	    status_e->status = status;
		list_push_back (curr_thread->parent_exit_list, &status_e->elem);
		if (!list_empty(&curr_thread->their_sema->waiters)){
			sema_up(curr_thread->their_sema);
		}
	}
	
	name_ptr = strtok_r (thread_current()->name, " ", &save_ptr);
	printf("%s: exit(%d)\n", name_ptr, status);
	
	thread_exit();
}

static pid_t syscall_exec (const char *cmd_line){
	pid_t pid;
	//printf("inside exec\n");
    //check for bad pointer
	if (!is_valid_pointer(cmd_line))
		syscall_exit(-1);
	
	pid = process_execute (cmd_line);
	return pid;
}

static int syscall_wait (pid_t pid){
  return process_wait(pid);
}

static bool syscall_create (const char *file, unsigned initial_size){
	//return !file ? syscall_exit(-1) : filesys_create (file, initial_size);
	bool created;
	if (!is_valid_pointer(file)){
		syscall_exit(-1);
	}
	else
		lock_acquire (&file_lock);
		created = filesys_create (file, initial_size);
		lock_release (&file_lock);
		return created;
}

static bool syscall_remove (const char *file){
	bool removed;
	if (!is_valid_pointer(file)){
		syscall_exit (-1); 
	}
	else
		lock_acquire (&file_lock);
		removed = filesys_remove (file);
		lock_release (&file_lock);
		return removed;
}

static int syscall_open (const char *file){
	struct file *process_file;
	struct fd_elem *fde;
	int status;

	status = -1;
	if (!is_valid_pointer(file)){
		syscall_exit (-1);
	}
	lock_acquire (&file_lock);	
	process_file = filesys_open (file);
	//file_deny_write (*process_file); //////////// 
	lock_release (&file_lock);
	if (!process_file)
		return status;

	fde = (struct fd_elem *)malloc (sizeof (struct fd_elem));
	if (!fde){
		//printf("Not enough memory\n");
		file_close (process_file);
		return status;
	}
	
	/* allocate fde an ID, put fde in file_list, put fde in the current thread's file_list */
	fde->file = process_file; 
	//TODO: write allocator for fd
	fde->fd = thread_current()->fd_counter++;
	list_push_back (&thread_current()->files, &fde->elem);
	status = fde->fd;
	return status;
}

static int syscall_filesize (int fd){
	struct file *process_file;
	int length;

	process_file = find_fd_elem (fd)->file;
	if (!process_file)
		syscall_exit(-1);

	lock_acquire (&file_lock);
	length = file_length(process_file);
	lock_release (&file_lock);
	return length;
}

static int syscall_read (int fd, void *buffer, unsigned size){
	struct file * process_file;
	unsigned i;
	int status = -1;

	if (fd > thread_current()->fd_counter)
		syscall_exit(-1);
	for (i = 0; i < size; i++){
		if (!is_valid_pointer((char *)buffer + i)){
			syscall_exit(-1);
		}
	}

	lock_acquire (&file_lock);
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

	if (fd > thread_current()->fd_counter)
		syscall_exit(-1);
	for (i = 0; i < size; i++){
		if (!is_valid_pointer((char *)buffer + i)){
			syscall_exit(-1);
		}
	}

	
	lock_acquire (&file_lock);
	status = -1;
	switch(fd){
		case -1:
			syscall_exit(-1);
			break;
		case STDIN_FILENO:
			lock_release (&file_lock);
			syscall_exit(-1);
			return status;
		case STDOUT_FILENO:
			putbuf (buffer, size);
			status = size;
			lock_release (&file_lock);
			return status;
		default:
			process_file = find_fd_elem (fd)->file;
			if (!process_file){
				lock_release (&file_lock);
				syscall_exit(-1);
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
	lock_acquire (&file_lock);
	file_seek (process_file, (off_t)position);
	lock_release (&file_lock);
}

static unsigned syscall_tell (int fd){
	struct file *process_file;
	int next_byte;
	
	//TODO: again, find file method
	process_file = find_fd_elem (fd)->file;
	if (!process_file)
		syscall_exit(-1);
	lock_acquire (&file_lock);
	next_byte = file_tell(process_file);
	lock_release (&file_lock);
	return next_byte;
}
static void syscall_close (int fd){
	struct fd_elem *fde;
  
	fde = find_fd_elem (fd);

	if (!fde || (fd == 0) || (fd == 1)) 
		syscall_exit (-1);

	lock_acquire (&file_lock);
	file_close (fde->file);
	lock_release (&file_lock);
	
	thread_current()->fd_counter--;
	list_remove (&fde->elem);
	free (fde);
}

////////////////////////////////////////////////////////////////////////////////////////
//Support methods
////////////////////////////////////////////////////////////////////////////////////////
static struct fd_elem * find_fd_elem (int fd){
	struct fd_elem *curr_elem;
	struct list_elem *e;
	
	e = list_begin(&thread_current()->files);
	while (e != list_end (&thread_current()->files)){
		curr_elem = list_entry(e, struct fd_elem, elem);
		if (curr_elem->fd == fd)
			return curr_elem;
		e = list_next(e);
	}
	
	return NULL;
}

static int is_valid_pointer (void *p){
	if ((p == NULL ) || !is_user_vaddr(p) || (pagedir_get_page(thread_current()->pagedir, p) == NULL))
		return 0;
	return 1;
}
