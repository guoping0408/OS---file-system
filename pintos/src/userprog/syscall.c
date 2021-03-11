#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame*);

static struct lock file_sys_lock;
static struct file* FDmap[100];

void syscall_init(void) { 
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); 
  lock_init(&file_sys_lock);
  
  for (int i = 0; i < 100; i++) {
    FDmap[i] = NULL;
  }
  
}

void check_validity(struct intr_frame* f, uint32_t* arg) {

  if (arg == NULL || !is_user_vaddr(arg) ||
      pagedir_get_page(thread_current()->pagedir, arg) == NULL || !is_user_vaddr(arg + 1) ||
      pagedir_get_page(thread_current()->pagedir, arg + 1) == NULL || !is_user_vaddr(arg + 2) ||
      pagedir_get_page(thread_current()->pagedir, arg + 2) == NULL || !is_user_vaddr(arg + 3) ||
      pagedir_get_page(thread_current()->pagedir, arg + 3) == NULL) {
    f->eax = -1;
    printf("%s: exit(%d)\n", &thread_current()->name, -1);
    
    // Do we free anything?
    thread_exit();
  }
}

int find_available_fd() {
  int idx = 2;
  while (idx < 100) {
    if (thread_current()->FDmap[idx] == NULL) {
      return idx;
    }
    idx++;
  }
  return -1;
}

bool valid_fd(int fd) {
  // We consider fd == 0 && fd == 1 are also invalid

  if (fd < 2 || fd >= 100 || thread_current()->FDmap[fd] == NULL) {
    return false;
  } else {
    return true;
  }

}


static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  check_validity(f, args);
  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  // syscall1(SYS_EXIT, status);
  if (args[0] == SYS_EXIT) {
    
    check_validity(f, &args[1]);
    thread_current()->process_tracker->exit_code = args[1];
    f->eax = args[1];
    printf("%s: exit(%d)\n", &thread_current()->name, args[1]);
    
    thread_exit();
  }

  //syscall1(SYS_PRACTICE, i)
  else if (args[0] == SYS_PRACTICE) {
    check_validity(f, &args[1]);
    f->eax = args[1] + 1;
  }

  // syscall0(SYS_HALT);
  else if (args[0] == SYS_HALT) {
    shutdown_power_off();
  }

  // pid_t exec(const char* file) { return (pid_t)syscall1(SYS_EXEC, file); }
  else if (args[0] == SYS_EXEC) {
    lock_acquire(&file_sys_lock);

    check_validity(f, args[1]);
    check_validity(f, &args[1]);
    
    f->eax = process_execute(args[1]);

    lock_release(&file_sys_lock);

  }

  //int wait(pid_t pid) { return syscall1(SYS_WAIT, pid); }
  else if (args[0] == SYS_WAIT) {
    check_validity(f, &args[1]);
    f->eax = process_wait(args[1]);
  }

  // HERE:
  // file system system call
  // syscall3(SYS_WRITE, fd, buffer, size);
  else if (args[0] == SYS_WRITE) {
    // if fd is the STDOUT
    // Fd 1 writes to the console. Your code to write to the console should write all of buffer in one call to putbuf()
    check_validity(f, &args[1]);
    check_validity(f, args[2]);
    check_validity(f, &args[2]);
    check_validity(f, &args[3]);

    lock_acquire(&file_sys_lock);

    int fd = args[1];
    if (fd == STDOUT_FILENO) {
      putbuf((const char*)args[2], args[3]);
      f->eax = args[3];
    }
    else {
      // if fd is something else
      if (!valid_fd(fd)) {
        f->eax = 0;
      }
      else {
        const char* buffer = args[2];
        long long size = args[3];

        // lock_acquire(&file_sys_lock);
        f->eax = file_write(thread_current()->FDmap[fd], buffer, size);
        // lock_release(&file_sys_lock);
        
      }
    }

    lock_release(&file_sys_lock);
  }

  // int read(int fd, void* buffer, unsigned size) {
  // return syscall3(SYS_READ, fd, buffer, size); }
  else if (args[0] == SYS_READ) {
    check_validity(f, &args[1]);
    check_validity(f, args[2]);
    check_validity(f, &args[2]);
    check_validity(f, &args[3]);
    int fd = args[1];

    lock_acquire(&file_sys_lock);
    void* buffer = args[2];
    if (fd == STDIN_FILENO) {
      // TODO
      // Read from Standard input (keyboard)
      uint8_t key = input_getc(); // read a single byte
      memcpy(buffer, &key, 1);
      f->eax = 1;

    } else {
      // We assume the maximum file descriptor is 99 for every process
      if (!valid_fd(fd)) {
        f->eax = -1;
      } else {
        struct file* toRead = thread_current()->FDmap[fd];
        f->eax = file_read(toRead, buffer, args[3]);
      }
    }
    lock_release(&file_sys_lock);
  }

  // bool create(const char* file, unsigned initial_size) {
  // return syscall2(SYS_CREATE, file, initial_size);
  // }
  else if (args[0] == SYS_CREATE) {
    check_validity(f, args[1]);
    check_validity(f, &args[1]);
    check_validity(f, &args[2]);

    lock_acquire(&file_sys_lock);
    char* file = args[1];
    unsigned init_size = args[2];
    if (file != NULL) {
      // lock_acquire(&file_sys_lock);
      f->eax = filesys_create(file, init_size); //(const char* name, off_t initial_size) {
      // lock_release(&file_sys_lock);
    } else {
      f->eax = -1;
    }

    lock_release(&file_sys_lock);

  }

  // bool remove(const char* file) { return syscall1(SYS_REMOVE, file); }
  else if (args[0] == SYS_REMOVE) {
    check_validity(f, args[1]);
    check_validity(f, &args[1]);
    char* file = args[1];

    lock_acquire(&file_sys_lock);
    f->eax = filesys_remove(file);
    lock_release(&file_sys_lock);
    
  }

  // int open(const char* file) { return syscall1(SYS_OPEN, file); }
  else if (args[0] == SYS_OPEN) {
    check_validity(f, args[1]);
    check_validity(f, &args[1]);

    lock_acquire(&file_sys_lock);
    struct file* openfile = filesys_open(args[1]);
    if (openfile == NULL) {
      f->eax = -1;
    } else {
      // Get the file descriptor
      
      int fd = find_available_fd();
      if (fd < 2) {
        f->eax = fd;
      } else {
        
        thread_current()->FDmap[fd] = openfile;
        f->eax = fd;
      }
    }
    lock_release(&file_sys_lock);
  }

  // int filesize(int fd) { return syscall1(SYS_FILESIZE, fd); }
  else if (args[0] == SYS_FILESIZE) {
    check_validity(f, &args[1]);
    int fd = args[1];
    
    lock_acquire(&file_sys_lock);
    if (!valid_fd(fd)) {
      f->eax = -1;
    } else {
      
      f->eax = file_length(thread_current()->FDmap[fd]);
      
    }
    lock_release(&file_sys_lock);

  }

  // void seek(int fd, unsigned position) { syscall2(SYS_SEEK, fd, position); }
  else if (args[0] == SYS_SEEK) {
    check_validity(f, &args[1]);
    check_validity(f, &args[2]);
    // TODO if position

    lock_acquire(&file_sys_lock);
    int fd = args[1];
    if (!valid_fd(fd)) {
      // TODO: what should we do here
    } else {
      file_seek(thread_current()->FDmap[fd], args[2]);
    }

    lock_release(&file_sys_lock);

  }

  // unsigned tell(int fd) { return syscall1(SYS_TELL, fd); }
  else if (args[0] == SYS_TELL) {
    check_validity(f, &args[1]);
    int fd = args[1];
    lock_acquire(&file_sys_lock);
    if (!valid_fd(fd)) {
      // TODO: what should we do here?
    } else {
      f->eax = file_tell(thread_current()->FDmap[fd]);
    }

    lock_release(&file_sys_lock);
  }

  // void close(int fd) { syscall1(SYS_CLOSE, fd); }
  else if (args[0] == SYS_CLOSE) {
    check_validity(f, &args[1]);
    lock_acquire(&file_sys_lock);
    int fd = args[1];
    if (!valid_fd(fd)) {
      // TODO: what should we do here?
    }
    else {
      file_close(thread_current()->FDmap[fd]);
      thread_current()->FDmap[fd] = NULL;
    }
    lock_release(&file_sys_lock);
  }
}
