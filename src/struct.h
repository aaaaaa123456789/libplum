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

struct data_node {
  union {
    struct {
      size_t size;
      struct data_node * previous;
      struct data_node * next;
    };
    max_align_t alignment;
  };
  unsigned char data[];
};

struct context {
  unsigned status;
  size_t size;
  union {
    const unsigned char * data;
    struct data_node * output; // reverse order: top of the list is the LAST node
  };
  union allocator_node * allocator;
  union {
    struct plum_image * image;
    const struct plum_image * source;
  };
  jmp_buf target;
};
