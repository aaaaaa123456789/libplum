#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include "../header/libplum.h"
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

// bmpwrite.c
internal void generate_BMP_data(struct context *);
internal void generate_BMP_bitmasked_data(struct context *, uint32_t, unsigned char *);
internal void generate_BMP_palette_halfbyte_data(struct context *, unsigned char *);
internal void generate_BMP_palette_byte_data(struct context *, unsigned char *);
internal size_t try_compress_BMP(struct context *, size_t, size_t (*) (uint8_t *, const uint8_t *, size_t));
internal size_t compress_BMP_halfbyte_row(uint8_t *, const uint8_t *, size_t);
internal unsigned emit_BMP_compressed_halfbyte_remainder(uint8_t *, const uint8_t *, unsigned);
internal size_t compress_BMP_byte_row(uint8_t *, const uint8_t *, size_t);
internal void append_BMP_palette(struct context *);
internal void generate_BMP_RGB_data(struct context *, unsigned char *);

// color.c
internal uint32_t get_true_color_depth(struct context *);

// framebuffer.c
internal void allocate_framebuffers(struct context *, unsigned, int);
internal void write_framebuffer_to_image(struct plum_image *, const uint64_t *, uint32_t, unsigned);
internal void write_palette_framebuffer_to_image(struct context *, const uint8_t *, const uint64_t *, uint32_t, unsigned, uint8_t);
internal void write_palette_to_image(struct context *, const uint64_t *, unsigned);
internal void rotate_frame_8(uint8_t * restrict, uint8_t * restrict, size_t, size_t, size_t (*) (size_t, size_t, size_t, size_t));
internal void rotate_frame_16(uint16_t * restrict, uint16_t * restrict, size_t, size_t, size_t (*) (size_t, size_t, size_t, size_t));
internal void rotate_frame_32(uint32_t * restrict, uint32_t * restrict, size_t, size_t, size_t (*) (size_t, size_t, size_t, size_t));
internal void rotate_frame_64(uint64_t * restrict, uint64_t * restrict, size_t, size_t, size_t (*) (size_t, size_t, size_t, size_t));
internal size_t rotate_left_coordinate(size_t, size_t, size_t, size_t);
internal size_t rotate_right_coordinate(size_t, size_t, size_t, size_t);
internal size_t rotate_both_coordinate(size_t, size_t, size_t, size_t);
internal size_t flip_coordinate(size_t, size_t, size_t, size_t);
internal size_t rotate_left_flip_coordinate(size_t, size_t, size_t, size_t);
internal size_t rotate_right_flip_coordinate(size_t, size_t, size_t, size_t);
internal size_t rotate_both_flip_coordinate(size_t, size_t, size_t, size_t);

// gifcompress.c
internal unsigned char * compress_GIF_data(struct context *, const unsigned char * restrict, size_t, size_t *, unsigned);
internal void decompress_GIF_data(struct context *, unsigned char * restrict, const unsigned char *, size_t, size_t, unsigned);
internal void initialize_GIF_compression_codes(struct compressed_GIF_code *, unsigned);
internal uint8_t find_leading_GIF_code(const struct compressed_GIF_code *, unsigned);
internal void emit_GIF_data(struct context *, const struct compressed_GIF_code *, unsigned, unsigned char **, unsigned char *);

// gifread.c
internal void load_GIF_data(struct context *, unsigned);
internal uint64_t ** load_GIF_palettes(struct context *, unsigned, size_t *, uint64_t *);
internal void load_GIF_palette(struct context *, uint64_t *, size_t *, unsigned);
internal void * load_GIF_data_blocks(struct context *, size_t *, size_t *);
internal void skip_GIF_data_blocks(struct context *, size_t *);
internal void load_GIF_loop_count(struct context *, size_t *);
internal void load_GIF_frame(struct context *, size_t *, unsigned, uint32_t, const uint64_t *, uint64_t, uint64_t * restrict, uint8_t * restrict);
internal void deinterlace_GIF_frame(struct context *, unsigned char * restrict, uint16_t, uint16_t);

// gifwrite.c
internal void generate_GIF_data(struct context *);
internal void generate_GIF_data_with_palette(struct context *, unsigned char *);
internal void generate_GIF_data_from_raw(struct context *, unsigned char *);
internal void generate_GIF_frame_data(struct context *, uint32_t * restrict, unsigned char * restrict, uint32_t, const struct plum_metadata *,
                                      const struct plum_metadata *);
internal int_fast32_t get_GIF_background_color(struct context *);
internal void write_GIF_palette(struct context *, const uint32_t *, unsigned);
internal void write_GIF_loop_info(struct context *);
internal void write_GIF_frame(struct context *, const unsigned char * restrict, const uint32_t *, unsigned, int, uint32_t, unsigned, unsigned, unsigned,
                              unsigned, const struct plum_metadata *, const struct plum_metadata *);
internal void write_GIF_data_blocks(struct context *, const unsigned char * restrict, size_t);

// load.c
internal void load_image_buffer_data(struct context *, unsigned);
internal void load_file(struct context *, const char *);
internal void load_from_callback(struct context *, const struct plum_callback *);

// metadata.c
internal struct plum_metadata * find_metadata(struct context *, int);
internal void add_color_depth_metadata(struct context *, unsigned, unsigned, unsigned, unsigned, unsigned);
internal void add_background_color_metadata(struct context *, uint64_t, unsigned);
internal void add_loop_count_metadata(struct context *, uint32_t);
internal void add_animation_metadata(struct context *, uint64_t **, uint8_t **);

// palette.c
internal void generate_palette(struct context *);
internal void remove_palette(struct context *);
internal uint64_t get_color_sorting_score(uint64_t, unsigned);
internal int compare64(const void *, const void *);

// store.c
internal void write_generated_image_data_to_file(struct context *, const char *);
internal void write_generated_image_data_to_callback(struct context *, const struct plum_callback *);
internal void write_generated_image_data(void * restrict, const struct data_node *);
internal size_t get_total_output_size(struct context *);

#include "inline.h"
