#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include "libplum.h"
#include "defs.h"
#include "struct.h"

#include "multibyte.h"

// allocator.c
internal void * allocate(union allocator_node **, size_t);
internal void * clear_allocate(union allocator_node **, size_t);
internal void deallocate(union allocator_node **, void *);
internal void * reallocate(union allocator_node **, void *, size_t);
internal void destroy_allocator_list(union allocator_node *);

// bmpread.c
internal void load_BMP_data(struct context *, unsigned);
internal uint8_t load_BMP_palette(struct context *, size_t, unsigned, uint64_t *);
internal void load_BMP_bitmasks(struct context *, size_t, uint8_t *, unsigned);
internal uint8_t * load_monochrome_BMP(struct context *, size_t, int);
internal uint8_t * load_halfbyte_BMP(struct context *, size_t, int);
internal uint8_t * load_byte_BMP(struct context *, size_t, int);
internal uint8_t * load_halfbyte_compressed_BMP(struct context *, size_t, int);
internal uint8_t * load_byte_compressed_BMP(struct context *, size_t, int);
internal uint64_t * load_BMP_pixels(struct context *, size_t, int, unsigned, uint64_t (*) (const unsigned char *, const void *), const void *);
internal uint64_t load_BMP_halfword_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_word_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_RGB_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_bitmasked_pixel(uint_fast32_t, const uint8_t *);

// framebuffer.c
internal void allocate_framebuffers(struct context *, unsigned);
internal void write_framebuffer_to_image(struct plum_image *, const uint64_t *, uint32_t, unsigned);
internal void write_palette_framebuffer_to_image(struct context *, const uint8_t *, const uint64_t *, uint32_t, unsigned, uint8_t);
internal void write_palette_to_image(struct context *, const uint64_t *, unsigned);

// load.c
internal void load_file(struct context *, const char *);

// metadata.c
internal void add_color_depth_metadata(struct context *, unsigned, unsigned, unsigned, unsigned, unsigned);

// palette.c
internal void generate_palette(struct context *);
internal void remove_palette(struct context *);
internal void remove_alpha(struct plum_image *);
internal uint64_t get_color_sorting_score(uint64_t, unsigned);
internal int compare64(const void *, const void *);

#include "inline.h"
