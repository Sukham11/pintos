#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

/* ---------------------------------------------------------------
   Task 3 — Safe user-memory helpers
   --------------------------------------------------------------- */

/* checks if user pointer is valid, else exits process */
static void
validate_user_ptr (const void *ptr)
{
  if (ptr == NULL                                          /* null pointer */
      || !is_user_vaddr (ptr)                             /* above PHYS_BASE */
      || pagedir_get_page (thread_current ()->pagedir,    /* unmapped page */
                           ptr) == NULL)
    {
      thread_current ()->exit_status = -1;
      thread_exit ();
    }
}

/* validate_user_range: ensures every byte in [BUFFER, BUFFER+SIZE)
   is a valid, mapped user address.  Used for write buffer checks. */
static void
validate_user_range (const void *buffer, unsigned size)
{
  const uint8_t *p = (const uint8_t *) buffer;
  unsigned i;
  for (i = 0; i < size; i++)
    validate_user_ptr (p + i);
}

/* copy_in_u32: safely reads a 32-bit word from user address UADDR.
   Validates the full 4-byte extent before dereferencing, so a bad
   stack pointer cannot crash the kernel. Returns the word value. */
static uint32_t
copy_in_u32 (const void *uaddr)
{
  /* check all 4 bytes of the 32-bit word */
  validate_user_ptr (uaddr);
  validate_user_ptr ((const uint8_t *) uaddr + 3);
  return *(const uint32_t *) uaddr;
}

/* ---------------------------------------------------------------
   Task 4.2 — syscall implementations
   --------------------------------------------------------------- */

/* Writes SIZE bytes from BUFFER to the open file FD.
Returns the number of bytes actually written, which may be
less than SIZE if some bytes could not be written. */
int
syscall_write (int fd, const void *buffer, unsigned size)
{
  /* For simplicity, we only handle writing to stdout (fd = 1). */
  if (fd != 1)
    return -1;

  /* Write to console output. */
  putbuf (buffer, size);
  return size;
}

/* ---------------------------------------------------------------
   Syscall initialisation and dispatch
   --------------------------------------------------------------- */

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles system calls */
static void
syscall_handler (struct intr_frame *f)
{
  /* --- Task 4.1 Step 1: validate the stack pointer itself --- */
  int *esp = (int *) f->esp;

  /* The syscall number sits at esp[0]; validate that word. */
  int syscall_number = (int) copy_in_u32 (esp);

  /* --- Task 4.1 Step 2: dispatch on syscall number --- */
  switch (syscall_number)
    {
    /* ---------------------------------------------------------
       Task 4.2 — halt()
       Shuts the machine down immediately; no return value.      */
    case SYS_HALT:
      shutdown_power_off ();
      break;

    /* ---------------------------------------------------------
       Task 4.2 — exit(int status)
       Stores STATUS, triggers the Task-1 exit message, and ends
       the calling process.  The kernel is never killed.         */
    case SYS_EXIT:
      {
        int status = (int) copy_in_u32 (esp + 1);

        /* Record status so process_exit() can print it (Task 1). */
        thread_current ()->exit_status = status;
        thread_exit ();
        break;
      }

    case SYS_WRITE:
      {
        int fd = (int) copy_in_u32 (esp + 1);
        const void *buffer = (const void *) copy_in_u32 (esp + 2);
        unsigned size = (unsigned) copy_in_u32 (esp + 3);

        validate_user_range (buffer, size); /* buffer must be readable */
        f->eax = syscall_write (fd, buffer, size);
        break;
      }

    /* terminate the process with -1.*/
    default:
      thread_current ()->exit_status = -1;
      thread_exit ();
    }
}
