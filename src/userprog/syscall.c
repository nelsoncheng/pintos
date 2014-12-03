#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  
  case SYS_CHDIR: 
    if (!is_valid_pointer(ARG1)){
  		syscall_exit(-1);
   	}
   	status = syscall_chdir((const char *) ARG1);
   	break;
  case SYS_MKDIR: 
    if (!is_valid_pointer(ARG1)){
  		syscall_exit(-1);
   	}
   	status = syscall_mkdir((const char *) ARG1);
   	break;
  case SYS_READDIR: 
    if (!is_valid_pointer(ARG1)){
  		syscall_exit(-1);
   	}
   	status = syscall_readdir((int) ARG1, (char *) ARG2);
   	break;
  case SYS_ISDIR: 
    if (!is_valid_pointer(ARG1)){
  		syscall_exit(-1);
   	}
   	status = syscall_isdir((int) ARG1);
   	break;
  case SYS_INUMBER: 
    if (!is_valid_pointer(ARG1)){
  		syscall_exit(-1);
   	}
   	status = syscall_inumber((int) ARG1);
    break;
  
  thread_exit ();
}
