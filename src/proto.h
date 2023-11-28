#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <setjmp.h>

#include "defs.h"
#include "../header/libplum.h"
#include "struct.h"

#include "data.h"
#include "multibyte.h"

#ifdef PLUM_DEBUG
  #define internal
#else
  #define internal static
#endif

// allocator.c
internal void * attach_allocator_node(struct allocator_node **, struct allocator_node *);
internal void * allocate(struct allocator_node **, size_t);
internal void * clear_allocate(struct allocator_node **, size_t);
internal void deallocate(struct allocator_node **, void *);
internal void * reallocate(struct allocator_node **, void *, size_t);
internal void destroy_allocator_list(struct allocator_node *);

// bmpread.c
internal void load_BMP_data(struct context *, unsigned long, size_t);
internal uint8_t load_BMP_palette(struct context *, size_t, unsigned, uint64_t * restrict);
internal void load_BMP_bitmasks(struct context *, size_t, uint8_t * restrict, unsigned);
internal uint8_t * load_monochrome_BMP(struct context *, size_t, bool);
internal uint8_t * load_halfbyte_BMP(struct context *, size_t, bool);
internal uint8_t * load_byte_BMP(struct context *, size_t, bool);
internal uint8_t * load_halfbyte_compressed_BMP(struct context *, size_t, bool);
internal uint8_t * load_byte_compressed_BMP(struct context *, size_t, bool);
internal uint64_t * load_BMP_pixels(struct context *, size_t, bool, size_t, uint64_t (*) (const unsigned char *, const void *), const void *);
internal uint64_t load_BMP_halfword_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_word_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_RGB_pixel(const unsigned char *, const void *);
internal uint64_t load_BMP_bitmasked_pixel(uint_fast32_t, const uint8_t *);

// bmpwrite.c
internal void generate_BMP_data(struct context *);
internal void generate_BMP_bitmasked_data(struct context *, uint32_t, unsigned char *);
internal void generate_BMP_palette_halfbyte_data(struct context *, unsigned char *);
internal void generate_BMP_palette_byte_data(struct context *, unsigned char *);
internal size_t try_compress_BMP(struct context *, size_t, size_t (*) (uint8_t * restrict, const uint8_t * restrict, size_t));
internal size_t compress_BMP_halfbyte_row(uint8_t * restrict, const uint8_t * restrict, size_t);
internal unsigned emit_BMP_compressed_halfbyte_remainder(uint8_t * restrict, const uint8_t * restrict, unsigned);
internal size_t compress_BMP_byte_row(uint8_t * restrict, const uint8_t * restrict, size_t);
internal void append_BMP_palette(struct context *);
internal void generate_BMP_RGB_data(struct context *, unsigned char *);

// checksum.c
internal uint32_t compute_PNG_CRC(const unsigned char *, size_t);
internal uint32_t compute_Adler32_checksum(const unsigned char *, size_t);

// color.c
internal bool image_has_transparency(const struct plum_image *);
internal uint32_t get_color_depth(const struct plum_image *);
internal uint32_t get_true_color_depth(const struct plum_image *);

// framebounds.c
internal struct plum_rectangle * get_frame_boundaries(struct context *, bool);
internal void adjust_frame_boundaries(const struct plum_image *, struct plum_rectangle * restrict);
internal bool image_rectangles_have_transparency(const struct plum_image *, const struct plum_rectangle *);

// framebuffer.c
internal void validate_image_size(struct context *, size_t);
internal void allocate_framebuffers(struct context *, unsigned long, bool);
internal void write_framebuffer_to_image(struct plum_image * restrict, const uint64_t * restrict, uint32_t, unsigned long);
internal void write_palette_framebuffer_to_image(struct context *, const uint8_t * restrict, const uint64_t * restrict, uint32_t, unsigned long, uint8_t);
internal void write_palette_to_image(struct context *, const uint64_t * restrict, unsigned long);
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

// frameduration.c
internal uint64_t adjust_frame_duration(uint64_t, int64_t * restrict);
internal void update_frame_duration_remainder(uint64_t, uint64_t, int64_t * restrict);
internal void calculate_frame_duration_fraction(uint64_t, uint32_t, uint32_t * restrict, uint32_t * restrict);

// gifcompress.c
internal unsigned char * compress_GIF_data(struct context *, const unsigned char * restrict, size_t, size_t * restrict, unsigned);
internal void decompress_GIF_data(struct context *, unsigned char * restrict, const unsigned char * restrict, size_t, size_t, unsigned);
internal void initialize_GIF_compression_codes(struct compressed_GIF_code * restrict, unsigned);
internal uint8_t find_leading_GIF_code(const struct compressed_GIF_code * restrict, unsigned);
internal void emit_GIF_data(struct context *, const struct compressed_GIF_code * restrict, unsigned, unsigned char **, unsigned char *);

// gifread.c
internal void load_GIF_data(struct context *, unsigned long, size_t);
internal uint64_t ** load_GIF_palettes_and_frame_count(struct context *, unsigned long, size_t * restrict, uint64_t * restrict);
internal void load_GIF_palette(struct context *, uint64_t * restrict, size_t * restrict, unsigned);
internal void * load_GIF_data_blocks(struct context *, size_t * restrict, size_t * restrict);
internal void skip_GIF_data_blocks(struct context *, size_t * restrict);
internal void load_GIF_frame(struct context *, size_t * restrict, unsigned long, uint32_t, const uint64_t * restrict, uint64_t, uint64_t * restrict,
                             uint8_t * restrict, struct plum_rectangle * restrict);

// gifwrite.c
internal void generate_GIF_data(struct context *);
internal void generate_GIF_data_with_palette(struct context *, unsigned char *);
internal void generate_GIF_data_from_raw(struct context *, unsigned char *);
internal void generate_GIF_frame_data(struct context *, uint32_t * restrict, unsigned char * restrict, uint32_t, const struct plum_metadata *,
                                      const struct plum_metadata *, int64_t * restrict, const struct plum_rectangle *);
internal int_fast32_t get_GIF_background_color(struct context *);
internal void write_GIF_palette(struct context *, const uint32_t * restrict, unsigned);
internal void write_GIF_loop_info(struct context *);
internal void write_GIF_frame(struct context *, const unsigned char * restrict, const uint32_t * restrict, unsigned, int, uint32_t, unsigned, unsigned, unsigned,
                              unsigned, const struct plum_metadata *, const struct plum_metadata *, int64_t * restrict);
internal void write_GIF_data_blocks(struct context *, const unsigned char * restrict, size_t);

// huffman.c
internal void generate_Huffman_tree(struct context *, const size_t * restrict, unsigned char * restrict, size_t, unsigned char);
internal void generate_Huffman_codes(unsigned short * restrict, size_t, const unsigned char * restrict, bool);

// jpegarithmetic.c
internal void decompress_JPEG_arithmetic_scan(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_decoder_tables *, size_t,
                                              const struct JPEG_component_info *, const size_t * restrict, unsigned, unsigned char, unsigned char, bool);
internal void decompress_JPEG_arithmetic_bit_scan(struct context *, struct JPEG_decompressor_state * restrict, size_t, const struct JPEG_component_info *,
                                                  const size_t * restrict, unsigned, unsigned char, unsigned char);
internal void decompress_JPEG_arithmetic_lossless_scan(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_decoder_tables *, size_t,
                                                       const struct JPEG_component_info *, const size_t * restrict, unsigned char, unsigned);
internal void initialize_JPEG_arithmetic_counters(struct context *, size_t * restrict, size_t * restrict, uint32_t * restrict);
internal int16_t next_JPEG_arithmetic_value(struct context *, size_t * restrict, size_t * restrict, uint32_t * restrict, uint16_t * restrict,
                                            unsigned char * restrict, signed char * restrict, unsigned, unsigned, unsigned char);
internal unsigned char classify_JPEG_arithmetic_value(uint16_t, unsigned char);
internal bool next_JPEG_arithmetic_bit(struct context *, size_t * restrict, size_t * restrict, signed char * restrict, uint32_t * restrict, uint16_t * restrict,
                                       unsigned char * restrict);

// jpegcomponents.c
internal uint32_t determine_JPEG_components(struct context *, size_t);
internal unsigned get_JPEG_component_count(uint32_t);
internal void (* get_JPEG_component_transfer_function(struct context *, const struct JPEG_marker_layout *, uint32_t))
               (uint64_t * restrict, size_t, unsigned, const double **);
internal void append_JPEG_color_depth_metadata(struct context *, void (*) (uint64_t * restrict, size_t, unsigned, const double **), unsigned);
internal void JPEG_transfer_RGB(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_BGR(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_ABGR(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_grayscale(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_alpha_grayscale(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_YCbCr(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_CbYCr(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_YCbCrK(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_CbKYCr(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_ACbYCr(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_CMYK(uint64_t * restrict, size_t, unsigned, const double **);
internal void JPEG_transfer_CKMY(uint64_t * restrict, size_t, unsigned, const double **);

// jpegcompress.c
internal struct JPEG_encoded_value * generate_JPEG_luminance_data_stream(struct context *, double (* restrict)[64], size_t, const uint8_t [restrict static 64],
                                                                         size_t * restrict);
internal struct JPEG_encoded_value * generate_JPEG_chrominance_data_stream(struct context *, double (* restrict)[64], double (* restrict)[64], size_t,
                                                                           const uint8_t [restrict static 64], size_t * restrict);
internal double generate_JPEG_data_unit(struct JPEG_encoded_value *, size_t * restrict, const double [restrict static 64], const uint8_t [restrict static 64],
                                        double);
internal void encode_JPEG_value(struct JPEG_encoded_value *, int16_t, unsigned, unsigned char);
internal size_t generate_JPEG_Huffman_table(struct context *, const struct JPEG_encoded_value *, size_t, unsigned char * restrict,
                                            unsigned char [restrict static 0x100], unsigned char);
internal void encode_JPEG_scan(struct context *, const struct JPEG_encoded_value *, size_t, const unsigned char [restrict static 0x200]);

// jpegdct.c
internal double apply_JPEG_DCT(int16_t [restrict static 64], const double [restrict static 64], const uint8_t [restrict static 64], double);
internal void apply_JPEG_inverse_DCT(double [restrict static 64], const int16_t [restrict static 64], const uint16_t [restrict static 64]);

// jpegdecompress.c
internal void initialize_JPEG_decompressor_state(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_component_info *,
                                                 const unsigned char *, size_t * restrict, size_t, size_t, size_t, unsigned char, unsigned char,
                                                 const struct JPEG_decoder_tables *, const size_t * restrict, int16_t (* restrict *)[64]);
internal void initialize_JPEG_decompressor_state_lossless(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_component_info *,
                                                          const unsigned char *, size_t * restrict, size_t, size_t, size_t, unsigned char, unsigned char,
                                                          const struct JPEG_decoder_tables *, const size_t * restrict, uint16_t * restrict *);
internal void initialize_JPEG_decompressor_state_common(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_component_info *,
                                                        const unsigned char *, size_t * restrict, size_t, size_t, size_t, unsigned char, unsigned char,
                                                        const struct JPEG_decoder_tables *, const size_t * restrict, unsigned char);
internal uint16_t predict_JPEG_lossless_sample(const uint16_t *, ptrdiff_t, bool, bool, unsigned, unsigned);

// jpeghierarchical.c
internal unsigned load_hierarchical_JPEG(struct context *, const struct JPEG_marker_layout *, uint32_t, double **);
internal void expand_JPEG_component_horizontally(struct context *, double * restrict, size_t, size_t, size_t, double * restrict);
internal void expand_JPEG_component_vertically(struct context *, double * restrict, size_t, size_t, size_t, double * restrict);
internal void normalize_JPEG_component(double * restrict, size_t, double);

// jpeghuffman.c
internal void decompress_JPEG_Huffman_scan(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_decoder_tables *, size_t,
                                           const struct JPEG_component_info *, const size_t * restrict, unsigned, unsigned char, unsigned char, bool);
internal void decompress_JPEG_Huffman_bit_scan(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_decoder_tables *, size_t,
                                               const struct JPEG_component_info *, const size_t * restrict, unsigned, unsigned char, unsigned char);
internal void decompress_JPEG_Huffman_lossless_scan(struct context *, struct JPEG_decompressor_state * restrict, const struct JPEG_decoder_tables *, size_t,
                                                    const struct JPEG_component_info *, const size_t * restrict, unsigned char, unsigned);
internal unsigned char next_JPEG_Huffman_value(struct context *, const unsigned char **, size_t * restrict, uint32_t * restrict, uint8_t * restrict,
                                               const short * restrict);

// jpegread.c
internal void load_JPEG_data(struct context *, unsigned long, size_t);
internal struct JPEG_marker_layout * load_JPEG_marker_layout(struct context *);
internal unsigned get_JPEG_rotation(struct context *, size_t);
internal unsigned load_single_frame_JPEG(struct context *, const struct JPEG_marker_layout *, uint32_t, double **);
internal unsigned char process_JPEG_metadata_until_offset(struct context *, const struct JPEG_marker_layout *, struct JPEG_decoder_tables *, size_t * restrict,
                                                          size_t);

// jpegreadframe.c
internal void load_JPEG_DCT_frame(struct context *, const struct JPEG_marker_layout *, uint32_t, size_t, struct JPEG_decoder_tables *, size_t * restrict,
                                  double **, unsigned, size_t, size_t);
internal void load_JPEG_lossless_frame(struct context *, const struct JPEG_marker_layout *, uint32_t, size_t, struct JPEG_decoder_tables *, size_t * restrict,
                                       double **, unsigned, size_t, size_t);
internal unsigned get_JPEG_component_info(struct context *, const unsigned char *, struct JPEG_component_info * restrict, uint32_t);
internal const unsigned char * get_JPEG_scan_components(struct context *, size_t, struct JPEG_component_info * restrict, unsigned, unsigned char * restrict);
internal void unpack_JPEG_component(double * restrict, double * restrict, size_t, size_t, size_t, size_t, unsigned char, unsigned char, unsigned char,
                                    unsigned char);

// jpegtables.c
internal void initialize_JPEG_decoder_tables(struct context *, struct JPEG_decoder_tables *, const struct JPEG_marker_layout *);
internal short * process_JPEG_Huffman_table(struct context *, const unsigned char ** restrict, uint16_t * restrict);
internal void load_default_JPEG_Huffman_tables(struct context *, struct JPEG_decoder_tables * restrict);

// jpegwrite.c
internal void generate_JPEG_data(struct context *);
internal void calculate_JPEG_quantization_tables(struct context *, uint8_t [restrict static 64], uint8_t [restrict static 64]);
internal void convert_JPEG_components_to_YCbCr(struct context *, double (* restrict)[64], double (* restrict)[64], double (* restrict)[64]);
internal void convert_JPEG_colors_to_YCbCr(const void * restrict, size_t, unsigned char, double * restrict, double * restrict, double * restrict,
                                           uint64_t * restrict);
internal void subsample_JPEG_component(double (* restrict)[64], double (* restrict)[64], size_t, size_t);

// load.c
internal void load_image_buffer_data(struct context *, unsigned long, size_t);
internal void prepare_image_buffer_data(struct context *, const void * restrict, size_t);
internal void load_file(struct context *, const char *);
internal void load_from_callback(struct context *, const struct plum_callback *);
internal void * resize_read_buffer(struct context *, void *, size_t * restrict);
internal void update_loaded_palette(struct context *, unsigned long);

// metadata.c
internal void add_color_depth_metadata(struct context *, unsigned, unsigned, unsigned, unsigned, unsigned);
internal void add_background_color_metadata(struct context *, uint64_t, unsigned long);
internal void add_loop_count_metadata(struct context *, uint32_t);
internal void add_animation_metadata(struct context *, uint64_t ** restrict, uint8_t ** restrict);
internal struct plum_rectangle * add_frame_area_metadata(struct context *);
internal uint64_t get_empty_color(const struct plum_image *);

// newstruct.c
internal struct context * create_context(void);

// palette.c
internal void generate_palette(struct context *, unsigned long);
internal void remove_palette(struct context *);
internal void sort_palette(struct plum_image *, unsigned long);
internal void apply_sorted_palette(struct plum_image *, unsigned long, const uint8_t *);
internal void reduce_palette(struct plum_image *);
internal unsigned check_image_palette(const struct plum_image *);
internal uint64_t get_color_sorting_score(uint64_t, unsigned long);

// pngcompress.c
internal unsigned char * compress_PNG_data(struct context *, const unsigned char * restrict, size_t, size_t, size_t * restrict);
internal struct compressed_PNG_code * generate_compressed_PNG_block(struct context *, const unsigned char * restrict, size_t, size_t, uint16_t * restrict,
                                                                    size_t * restrict, size_t * restrict, bool);
internal size_t compute_uncompressed_PNG_block_size(const unsigned char * restrict, size_t, size_t, uint16_t * restrict);
internal unsigned find_PNG_reference(const unsigned char * restrict, const uint16_t * restrict, size_t, size_t, size_t * restrict);
internal void append_PNG_reference(const unsigned char * restrict, size_t, uint16_t * restrict);
internal uint16_t compute_PNG_reference_key(const unsigned char * data);
internal void emit_PNG_code(struct context *, struct compressed_PNG_code **, size_t * restrict, size_t * restrict, int, unsigned);
internal unsigned char * emit_PNG_compressed_block(struct context *, const struct compressed_PNG_code * restrict, size_t, bool, size_t * restrict,
                                                   uint32_t * restrict, uint8_t * restrict);
internal unsigned char * generate_PNG_Huffman_trees(struct context *, uint32_t * restrict, uint8_t * restrict, size_t * restrict,
                                                    const size_t [restrict static 0x120], const size_t [restrict static 0x20],
                                                    unsigned char [restrict static 0x120], unsigned char [restrict static 0x20]);

// pngdecompress.c
internal void * decompress_PNG_data(struct context *, const unsigned char *, size_t, size_t);
internal void extract_PNG_code_table(struct context *, const unsigned char **, size_t * restrict, unsigned char [restrict static 0x140], uint32_t * restrict,
                                     uint8_t * restrict);
internal void decompress_PNG_block(struct context *, const unsigned char **, unsigned char * restrict, size_t * restrict, size_t * restrict, size_t,
                                   uint32_t * restrict, uint8_t * restrict, const unsigned char [restrict static 0x140]);
internal short * decode_PNG_Huffman_tree(struct context *, const unsigned char *, unsigned);
internal uint16_t next_PNG_Huffman_code(struct context *, const short * restrict, const unsigned char **, size_t * restrict, uint32_t * restrict,
                                        uint8_t * restrict);

// pngread.c
internal void load_PNG_data(struct context *, unsigned long, size_t);
internal struct PNG_chunk_locations * load_PNG_chunk_locations(struct context *);
internal void append_PNG_chunk_location(struct context *, size_t **, size_t, size_t * restrict);
internal void sort_PNG_animation_chunks(struct context *, struct PNG_chunk_locations * restrict, const size_t * restrict, size_t, size_t);
internal uint8_t load_PNG_palette(struct context *, const struct PNG_chunk_locations * restrict, uint8_t, uint64_t * restrict);
internal void add_PNG_bit_depth_metadata(struct context *, const struct PNG_chunk_locations *, uint8_t, uint8_t);
internal uint64_t add_PNG_background_metadata(struct context *, const struct PNG_chunk_locations *, const uint64_t *, uint8_t, uint8_t, uint8_t, unsigned long);
internal uint64_t load_PNG_transparent_color(struct context *, size_t, uint8_t, uint8_t);
internal bool check_PNG_reduced_frames(struct context *, const struct PNG_chunk_locations *);
internal void load_PNG_animation_frame_metadata(struct context *, size_t, uint64_t * restrict, uint8_t * restrict);

// pngreadframe.c
internal void load_PNG_frame(struct context *, const size_t *, uint32_t, const uint64_t *, uint8_t, uint8_t, uint8_t, bool, uint64_t, uint64_t);
internal void * load_PNG_frame_part(struct context *, const size_t *, int, uint8_t, uint8_t, bool, uint32_t, uint32_t, size_t);
internal uint8_t * load_PNG_palette_frame(struct context *, const void *, size_t, uint32_t, uint32_t, uint8_t, uint8_t, bool);
internal uint64_t * load_PNG_raw_frame(struct context *, const void *, size_t, uint32_t, uint32_t, uint8_t, uint8_t, bool);
internal void load_PNG_raw_frame_pass(struct context *, unsigned char * restrict, uint64_t * restrict, uint32_t, uint32_t, uint32_t, uint8_t, uint8_t,
                                      unsigned char, unsigned char, unsigned char, unsigned char, size_t);
internal void expand_bitpacked_PNG_data(unsigned char * restrict, const unsigned char * restrict, size_t, uint8_t);
internal void remove_PNG_filter(struct context *, unsigned char * restrict, uint32_t, uint32_t, uint8_t, uint8_t);

// pngwrite.c
internal void generate_PNG_data(struct context *);
internal void generate_APNG_data(struct context *);
internal unsigned generate_PNG_header(struct context *, struct plum_rectangle * restrict);
internal void append_PNG_header_chunks(struct context *, unsigned, uint32_t);
internal void append_PNG_palette_data(struct context *, bool);
internal void append_PNG_background_chunk(struct context *, const void * restrict, unsigned);
internal void append_PNG_image_data(struct context *, const void * restrict, unsigned, uint32_t * restrict, const struct plum_rectangle *);
internal void append_APNG_frame_header(struct context *, uint64_t, uint8_t, uint32_t * restrict, int64_t * restrict, const struct plum_rectangle *);
internal void output_PNG_chunk(struct context *, uint32_t, uint32_t, const void * restrict);
internal unsigned char * generate_PNG_frame_data(struct context *, const void * restrict, unsigned, size_t * restrict, const struct plum_rectangle *);
internal void generate_PNG_row_data(struct context *, const void * restrict, unsigned char * restrict, size_t, unsigned);
internal void filter_PNG_rows(unsigned char * restrict, const unsigned char * restrict, size_t, unsigned);
internal unsigned char select_PNG_filtered_row(const unsigned char *, size_t);

// pnmread.c
internal void load_PNM_data(struct context *, unsigned long, size_t);
internal void load_PNM_header(struct context *, size_t, struct PNM_image_header * restrict);
internal void load_PAM_header(struct context *, size_t, struct PNM_image_header * restrict);
internal void skip_PNM_whitespace(struct context *, size_t * restrict);
internal void skip_PNM_line(struct context *, size_t * restrict);
internal unsigned next_PNM_token_length(struct context *, size_t);
internal void read_PNM_numbers(struct context *, size_t * restrict, uint32_t * restrict, size_t);
internal void add_PNM_bit_depth_metadata(struct context *, const struct PNM_image_header *);
internal void load_PNM_frame(struct context *, const struct PNM_image_header * restrict, uint64_t * restrict);
internal void load_PNM_bit_frame(struct context *, size_t, size_t, size_t, uint64_t * restrict);

// pnmwrite.c
internal void generate_PNM_data(struct context *);
internal uint32_t * get_true_PNM_frame_sizes(struct context *);
internal void generate_PPM_data(struct context *, const uint32_t * restrict, unsigned, uint64_t * restrict);
internal void generate_PPM_header(struct context *, uint32_t, uint32_t, unsigned);
internal void generate_PAM_data(struct context *, const uint32_t * restrict, unsigned, uint64_t * restrict);
internal void generate_PAM_header(struct context *, uint32_t, uint32_t, unsigned);
internal size_t write_PNM_number(unsigned char * restrict, uint32_t);
internal void generate_PNM_frame_data(struct context *, const uint64_t *, uint32_t, uint32_t, unsigned, bool);
internal void generate_PNM_frame_data_from_palette(struct context *, const uint8_t *, const uint64_t *, uint32_t, uint32_t, unsigned, bool);

// qoiread.c
internal void load_QOI_data(struct context *, unsigned, size_t);

// qoiwrite.c
internal void generate_QOI_data(struct context *);

// sort.c
internal void sort_values(uint64_t * restrict, uint64_t);
internal void quicksort_values(uint64_t * restrict, uint64_t);
internal void merge_sorted_values(uint64_t * restrict, uint64_t, uint64_t * restrict);
internal void sort_pairs(struct pair * restrict, uint64_t);
internal void quicksort_pairs(struct pair * restrict, uint64_t);
internal void merge_sorted_pairs(struct pair * restrict, uint64_t, struct pair * restrict);

// store.c
internal void write_generated_image_data_to_file(struct context *, const char *);
internal void write_generated_image_data_to_callback(struct context *, const struct plum_callback *);
internal void write_generated_image_data(void * restrict, const struct data_node *);
internal size_t get_total_output_size(struct context *);

#undef internal

#include "inline.h"
