#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
	char * name_ptr, save_ptr;

  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      //printf ("%s: dying due to interrupt %#04x (%s).\n",
              //thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f); 
     //Jonathan driving
	   int retval;                                                    
          asm volatile                                                   
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" 
               : "=a" (retval)                                           
               : [number] "i" (1),                                  
                 [arg0] "g" (-1)                                       
               : "memory");                                              
          retval;

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  uint8_t frame;
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr, *page, *sup_pte, *stack_ptr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();
  
  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) !=  
  if (!is_user_vaddr(fault_addr)){
  	f->eip = (void (*) (void)) f->eax;
	f->eax = 0xffffffff;
  	//do the magical syscall_exiting if the fault address is unmapped and not a stack access, hope this works
  	 int retval;                                                    
          asm volatile                                                   
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" 
               : "=a" (retval)                                           
               : [number] "i" (1),                                  
                 [arg0] "g" (-1)                                       
               : "memory");                                              
          retval;
  }
  
  
  //get the page that contains the faulting virtual address
  uint32_t faulting_address = (uint32_t) fault_addr;
  page = (void*) (faulting_address & PTE_ADDR);
  //get the supplemental page
  sup_pte = pagedir_get_page(thread_current()->pagedir, page);
  //get the appropriate stack pointer (kernel or user)
  if (f->cs == SEL_UCSEG){
  	stack_ptr = f->esp;
  } else {
  	stack_ptr = thread_current()->kernel_stack_pointer;
  }
  if (sup_pte == NULL || !stack_page_fault(stack_ptr, fault_addr)){
  	//do the magical syscall_exiting if the fault address is unmapped and not a stack access, hope this works
  	 int retval;                                                    
          asm volatile                                                   
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" 
               : "=a" (retval)                                           
               : [number] "i" (1),                                  
                 [arg0] "g" (-1)                                       
               : "memory");                                              
          retval;
  } 
  if (sup_pte == NULL){
  	//if we get in here then we need a new stack page
  	frame = frame_get(page, true, NULL);
  	pagedir_clear_page(thread_current()->pagedir, page);
  	bool success = pagedir_set_page(thread_current()->pagedir, page, frame, true);
  	if (!success){
  		//change later
  		PANIC("For some reason pagedir_set_page didnt work");
  	}
  	pagedir_set_accessed(thread_current()->pagedir, page, true);
  	pagedir_set_dirty(thread_current()->pagedir, page, true);
  	return;
  }
  //if the fault wasnt a stack access, then the supplemental page becomes relevant
  struct pte * supplemental_pte = (struct pte *) sup_pte;
  frame = frame_get(page, true, supplemental_pte);
  bool dirty_bit, success;
  
  if (supplemental_pte->ptype == EXECUTABLE_PAGE){
  	dirty = false;
  	syscall_file_lock_acquire();
  	if (file_read (supplemental_pte->file_ptr, frame, supplemental_pte->bytes_to_read) != (int) supplemental_pte->bytes_to_read)
	{
		syscall_file_lock_release();
		palloc_free_page (frame);
		PANIC ("File_read in page fault handler didnt read enough bytes");
	}
	syscall_file_lock_release();
  } else if (supplemental_pte->ptype == MMAP_FILE_PAGE){
  	//extra credit?
  	printf("how did we even get here (mmap file page)\n");
  } else if (supplemental_pte->ptype == ZERO_PAGE){
  	memset (frame, 0, PGSIZE);
  	dirty_bit = false;
  } else if (supplemental_pte->ptype == SWAP_PAGE){
  	frame_pin(frame, true, true);
  	swap_retrieve(supplemental_pte->member, frame);
  	frame_pin(frame, true, false);
  	dirty = true;
  } else {
  	PANIC ("Supplemental page didnt have a type");
  }
  pagedir_clear_page(thread_current()->pagedir, page);
  success = pagedir_set_page(thread_current()->pagedir, page, frame, true);
  if (!success){
  	//change later
  	PANIC("For some reason pagedir_set_page didnt work");
  }
  pagedir_set_accessed(thread_current()->pagedir, page, true);
  pagedir_set_dirty(thread_current()->pagedir, page, dirty_bit);
}


//Page fault helper methods:

bool stack_page_fault(void * stack_ptr, void * fault_address){
	bool success = true;
	if (fault_address > PHYS_BASE)
		success = false;
	if (address < STACK_BOTTOM)
		success = false;
	if ((stack_ptr - 32) > fault_address)
		success = false;
	return success;
}


