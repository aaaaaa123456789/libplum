#ifndef LIBPLUM_H

#define LIBPLUM_H

#include <stdint.h>
#include <stddef.h>

#if !defined(__cplusplus) && (__STDC_VERSION__ >= 199901L)
/* C99 or later, not C++, we can use restrict, and check for VLAs and anonymous struct members (C11) */
/* indented preprocessor directives and // comments are also allowed here, but we'll avoid them for consistence */
#define PLUM_RESTRICT restrict
#define PLUM_ANON_MEMBERS (__STDC_VERSION__ >= 201112L)
/* protect against really broken preprocessor implementations */
#define PLUM_VLA_SUPPORT (!defined(__STDC_NO_VLA__) || !(__STDC_NO_VLA__ + 0))
/* ... */
#else
/* C89 or C++ (or, if we're really unlucky, non-standard C), so don't use any "advanced" C features */
#define PLUM_RESTRICT
#define PLUM_ANON_MEMBERS 0
#define PLUM_VLA_SUPPORT 0
#endif

#include "enum.h"
#include "color.h"

struct plum_buffer {
  size_t size;
  void * data;
};

struct plum_metadata {
  int type;
  size_t size;
  void * data;
  struct plum_metadata * next;
};

struct plum_image {
  uint16_t type;
  uint8_t max_palette_index;
  uint8_t color_format;
  uint32_t frames;
  uint32_t height;
  uint32_t width;
  void * allocator;
  struct plum_metadata * metadata;
#if PLUM_ANON_MEMBERS
  union {
#endif
    void * palette;
#if PLUM_ANON_MEMBERS
    uint16_t * palette16;
    uint32_t * palette32;
    uint64_t * palette64;
  };
  union {
#endif
    void * data;
#if PLUM_ANON_MEMBERS
    uint8_t * data8;
    uint16_t * data16;
    uint32_t * data32;
    uint64_t * data64;
  };
#endif
  void * user;
};

/* keep declarations readable: redefine the "restrict" keyword, and undefine it later
   (note that, if this expands to "#define restrict restrict", that will NOT expand recursively) */
#define restrict PLUM_RESTRICT

#ifdef __cplusplus
extern "C" {
#endif

struct plum_image * plum_new_image(void);
struct plum_image * plum_copy_image(const struct plum_image * image);
void plum_destroy_image(struct plum_image * image);
struct plum_image * plum_load_image(const void * restrict buffer, size_t size, unsigned flags, unsigned * restrict error);
unsigned plum_store_image(const struct plum_image * image, void * restrict buffer, size_t size);
const char * plum_get_error_text(unsigned error);
int plum_check_valid_image_size(uint32_t width, uint32_t height, uint32_t frames);
size_t plum_color_buffer_size(size_t size, unsigned flags);
void plum_convert_colors(void * restrict destination, const void * restrict source, size_t count, unsigned to, unsigned from);
uint64_t plum_convert_color(uint64_t color, unsigned from, unsigned to);
int plum_convert_colors_to_indexes(uint8_t * restrict destination, const void * restrict source, void * restrict palette, size_t count, unsigned flags);
void plum_convert_indexes_to_colors(void * restrict destination, const uint8_t * restrict source, const void * restrict palette, size_t count, unsigned flags);
void * plum_malloc(struct plum_image * image, size_t size);
void * plum_calloc(struct plum_image * image, size_t size);
void * plum_realloc(struct plum_image * image, void * buffer, size_t size);
void plum_free(struct plum_image * image, void * buffer);
struct plum_metadata * plum_allocate_metadata(struct plum_image * image, size_t size);

#ifdef __cplusplus
}
#endif

#undef restrict

#endif
