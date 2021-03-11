#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>
#include "lib/kernel/list.h"
#include <stdint.h>

void syscall_init(void);



typedef struct file_descriptor {
  int fd;
  struct file* file;
  struct list_elem elem;
} file_descriptor_t;

typedef struct fd_store {
  file_descriptor_t** store; //Double starred because it is an array
  int size;
  struct list* free_fd_queue;
} fd_store_t;

// Creates a File Descriptor Storage Struct
struct fd_store* create_fd_store();

#endif /* userprog/syscall.h */
