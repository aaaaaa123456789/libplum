#include <stddef.h>
#include <setjmp.h>

#include "../header/libplum.h"

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

struct compressed_GIF_code {
  alignas(uint32_t) int16_t reference; // align the first member to align the struct
  unsigned char value;
  unsigned char type;
};

struct PNG_chunk_locations {
  // includes APNG chunks; IHDR and IEND omitted because IHDR has a fixed offset and IEND contains no data
  size_t palette; // PLTE
  size_t bits; // sBIT
  size_t background; // bKGD
  size_t transparency; // tRNS
  size_t animation; // acTL
  size_t * data; // IDAT
  size_t * frameinfo; // fcTL
  size_t ** framedata; // fdAT
};

struct compressed_PNG_code {
  // members declared as uint32_t to hint at compilers to use 32-bit integers
  uint32_t datacode:   9;
  uint32_t dataextra:  5;
  uint32_t distcode:   5;
  uint32_t distextra: 13;
};
