#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"

#define LOWER_LIMITS ((void *) 0x08048000)
#define STDOUT_FILENO 1
#define STDIN_FILENO 0
#define ERROR -1

static void syscall_handler (struct intr_frame *);
static void check_valid_ptr (void *vaddr);
static void* check_addr (void *addr, unsigned size);
struct lock sync;

void
syscall_init (void) 
{
	lock_init (&sync);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static struct file*
get_file (int _fd)
{
	struct list_elem *e;
	struct list *list = &thread_current ()->fd_table;
  	for (e = list_begin (list); e != list_end (list); e = list_next (e))
  	{
  		struct file *f = list_entry (e, struct file, elem);
  		if (f->fd == _fd)
  		{
  			return f;
  		}
  	}
  	return NULL;
}

/* System Call: void exit (int status) */
void 
exit (int status)
{
	thread_current ()->child_process->exit_status = status;
	sema_up (&thread_current ()->child_process->sema_child);
	thread_exit ();
}

/* System Call: void halt (void) */
static void 
halt (void)
{
	shutdown_power_off ();
}

/*  check that the a process waits for any given child at most once 
    and pid is a direct child of the calling process. */
static struct child_process*
get_child (tid_t child_tid)
{
  struct list_elem *e;
  struct list *list = &thread_current ()->child_list;
  for (e = list_begin (list); e != list_end (list); e = list_next (e))
  {
    struct child_process *cp = list_entry (e, struct child_process, elem);
    if (cp->tid == child_tid && !cp->parent_iswaiting)
      return cp;
  }
  return NULL;
}

/* System Call: pid_t exec (const char *cmd_line) */
static pid_t 
exec (const char *cmd_line) 
{
	cmd_line = check_addr (cmd_line,1);
	lock_acquire (&sync);
	tid_t id = process_execute (cmd_line);
	lock_release (&sync);
	// Failed to execute child.
	if (id == TID_ERROR){
		return TID_ERROR;
	}
	else
	{
		struct child_process *child_process = get_child (id);
		sema_down (&child_process->sync);
		return child_process->tid;
	}
}

/* System Call: int wait (pid_t pid) */
static int 
wait (pid_t pid)
{
	int child_exit_status = process_wait (pid);
	free (get_child (pid));
	return child_exit_status;
}

/* System Call: bool create (const char *file, unsigned initial_size) */
static bool
create (const char *file, unsigned initial_size)
{
	if (!file)
		exit (-1);
	file = check_addr (file,1);
	lock_acquire (&sync);
	bool ans = filesys_create (file, initial_size);
	lock_release (&sync);
	return ans;
}

/* System Call: bool remove (const char *file) */
static bool 
remove (const char *file)
{
	lock_acquire (&sync);
	bool ans = filesys_remove (file);
	lock_release (&sync);
	return ans;
}

static int
get_fd (void)
{
	int fd = random_ulong() % INT32_MAX;
	fd &= ((1 << 31) - 1); // from negative to positive int.
	while (fd <= 1)
	{
		fd = random_ulong() % INT32_MAX;
		fd &= ((1 << 31) - 1);
	}
	return fd;
}

/* System Call: int open (const char *file) */
static int 
open (const char *file)
{
	if(!file)
		exit (-1);
	file = check_addr (file, 1);
	lock_acquire (&sync);
	struct file *new_file = filesys_open (file);
	lock_release (&sync);
	if (!new_file)
		return -1;
	new_file->fd = get_fd ();
    list_push_back (&thread_current ()->fd_table,&new_file->elem);
	return new_file->fd;
}

/* System Call: int filesize (int fd) */
static int 
filesize (int fd)
{
	struct file *f = get_file (fd);
	if (!f)
		exit (-1);
	return file_length (f);
}


/* System Call: int read (int fd, void *buffer, unsigned size) */
static int 
read (int fd, void *buffer, unsigned size)
{
	check_valid_ptr (buffer);
	if (fd == STDIN_FILENO)
	{
		return input_getc ();
	}
	else
	{
		struct file *f = get_file (fd);
		if (!f)
			exit (-1);
		lock_acquire (&sync);
		int ans = file_read (f, buffer ,size);
		lock_release (&sync);
		return ans;
	}
}

/* System Call: int write (int fd, const void *buffer, unsigned size) */
static int 
write (int fd, const void *buffer, unsigned size)
{
	buffer = check_addr (buffer,size);
	if (fd == STDOUT_FILENO)
	{
		putbuf(buffer,size);
		return size;
	}
	else
	{
		struct file *file = get_file (fd);
		if (!file)
			return ERROR;
		if (is_executable_file (file->file_name))
			return 0;
		lock_acquire (&sync);
		int ans = file_write (file,buffer,size);
		lock_release (&sync);
 		return ans;
	}
}

/* System Call: void seek (int fd, unsigned position) */
static void 
seek (int fd, unsigned position)
{
	file_seek (get_file (fd), position);
}

/* System Call: unsigned tell (int fd) */
static unsigned 
tell (int fd)
{
	return file_tell (get_file (fd));
}

/* System Call: void close (int fd) */
static void 
close (int fd)
{
	struct file *file = get_file (fd);
	if (!file)
		exit (-1);
	list_remove (&file->elem);
	lock_acquire (&sync);
	file_close (file);
	lock_release (&sync);
}

static void 
check_valid_ptr (void *vaddr)
{
  if (!is_user_vaddr(vaddr) || vaddr < LOWER_LIMITS)
    exit(ERROR);
}

/* checking the validation of the address and return the kernel address correspond
to the user virtual address */
static void*
check_addr (void *addr, unsigned size)
{
	char* local_buffer = (char *) addr;
  	for (int i = 0; i < size; i++)
    {
      check_valid_ptr((void*) local_buffer);
      local_buffer++;
    }
	void *address = pagedir_get_page (thread_current ()->pagedir , addr);
	if (address != NULL)
	  return address;
	exit(ERROR);
}

static void 
read_arguments (int *stack,int *argv,int size)
{
	for(int i = 0 ;i < size; i++)
	{
		check_valid_ptr (stack + i + 1);
		argv[i] = *(stack + i + 1);
	}
}

static void
syscall_handler (struct intr_frame *f) 
{
	check_addr (f->esp, 1);
	int* stack = (int *) f->esp;
	int* argv[3];
  switch (*stack)
  {
  	case SYS_HALT:				/* Halt the operating system. */
  	{
  		//printf("SYS_ Halt %d\n", SYS_HALT);
  		halt ();
  		break;
  	}
  	  	case SYS_EXIT:			/* Terminate this process. */
  	{
  		// printf("SYS_ EXIT %d\n", SYS_EXIT);
  		read_arguments (stack, argv , 1);
  		exit (argv[0]);
  		break;
  	}
  	  	case SYS_EXEC:			/* Start another process. */
  	{
  		// printf("SYS_EXEC %d\n", SYS_EXEC);
  		read_arguments (stack, argv, 1);
  		f->eax = exec (argv[0]); 
  		break;
  	}
  	  	case SYS_WAIT:			/* Wait for a child process to die. */
  	{
  		// printf("SYS_WAIT %d\n", SYS_WAIT);
  		read_arguments (stack , argv ,1);
  		f->eax = wait (argv[0]);
  		break;
  	}
  	  	case SYS_CREATE:		/* Create a file. */
  	{
  		//printf("SYS_CREATE %d\n", SYS_CREATE);
  		read_arguments (stack , argv , 2);
  		f->eax = create (argv[0] , argv[1]);
  		break;
  	}
  	  	case SYS_REMOVE:		/* Delete a file. */
  	{
  		//printf("SYS_REMOVE %d\n", SYS_REMOVE);
  		read_arguments (stack , argv , 1);
  		f->eax = remove (argv[0]);
  		break;
  	}
  	  	case SYS_OPEN:			/* Open a file. */
  	{
  		// printf("SYS_OPEN %d\n", SYS_OPEN);
  		read_arguments (stack , argv, 1);
  		f->eax = open (argv[0]);
  		break;
  	}
  	  	case SYS_FILESIZE:		/* Obtain a file's size. */
  	{
  		//printf("SYS_FILESIZE %d\n", SYS_FILESIZE);
  		read_arguments (stack , argv ,1);
  		f->eax = filesize (argv[0]);
  		break;
  	}
  	  	case SYS_READ:			/* Read from a file. */
  	{
  		// printf("SYS_READ %d\n", SYS_READ);
  		read_arguments (stack , argv , 3);
  		f->eax = read (argv[0], argv[1], argv[2]);
  		break;
  	}
  	  	case SYS_WRITE:			/* Write to a file. */
  	{
  		//printf("SYS_WRITE %d\n", SYS_WRITE);
  		// //printf("%d\n", *(int*)(stack));
  		read_arguments (stack , argv , 3);
  		f->eax = write (argv[0],argv[1],argv[2]);
  		break;
  	}
  	  	case SYS_SEEK:			/* Change position in a file. */
  	{
  		//printf("SYS_SEEK %d\n", SYS_SEEK);
  		read_arguments (stack, argv, 2);
  		seek (argv[0], argv[1]);
  		break;
  	}
  	  	case SYS_TELL:			/* Report current position in a file. */
  	{
  		//printf("SYS_TELL %d\n", SYS_TELL);
  		read_arguments (stack, argv, 1);
  		f->eax = tell (argv[0]);
  		break;
  	}
  	  	case SYS_CLOSE:			/* Close a file. */
  	{
  		//printf("SYS_CLOSE %d\n", SYS_CLOSE);
  		read_arguments (stack , argv , 1);
  		close (argv[0]);
  		break;
  	}
  }
}