#include <stddef.h>
#include <setjmp.h>

#include "libplum.h"

union allocator_node {
  max_align_t alignment;
  struct {
    union allocator_node * previous;
    union allocator_node * next;
  };
};

struct context {
  unsigned status;
  size_t size;
  const unsigned char * data;
  union allocator_node * allocator;
  struct plum_image * image;
  jmp_buf target;
};
