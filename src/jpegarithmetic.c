#include "proto.h"

void decompress_JPEG_arithmetic_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                      size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                      unsigned char last) {
  // ...
}

void decompress_JPEG_arithmetic_bit_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                          size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift,
                                          unsigned char first, unsigned char last) {
  if (last && !first) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // ...
}
