#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "lib/user/syscall.h"

void syscall_init (void);
static void syscall_handler (struct intr_frame *);

void syscall_file_lock_acquire();
void syscall_file_lock_release();
#endif /* userprog/syscall.h */
