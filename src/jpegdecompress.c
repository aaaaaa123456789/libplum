#include "proto.h"

void initialize_JPEG_decompressor_state (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_component_info * components,
                                         const unsigned char * componentIDs, size_t * restrict unitsH, size_t unitsV, size_t width, size_t height,
                                         unsigned char maxH, unsigned char maxV, const struct JPEG_decoder_tables * tables, const size_t * offsets,
                                         int16_t (* restrict * output)[64]) {
  initialize_JPEG_decompressor_state_common(context, state, components, componentIDs, unitsH, unitsV, width, height, maxH, maxV, tables, offsets, 8);
  unsigned char p;
  for (p = 0; p < state -> component_count; p ++) state -> current_block[componentIDs[p]] = output[componentIDs[p]];
  for (; p < 4; p ++) state -> current_block[p] = NULL;
}

void initialize_JPEG_decompressor_state_lossless (struct context * context, struct JPEG_decompressor_state * restrict state,
                                                  const struct JPEG_component_info * components, const unsigned char * componentIDs, size_t * restrict unitsH,
                                                  size_t unitsV, size_t width, size_t height, unsigned char maxH, unsigned char maxV,
                                                  const struct JPEG_decoder_tables * tables, const size_t * offsets, uint16_t * restrict * output) {
  initialize_JPEG_decompressor_state_common(context, state, components, componentIDs, unitsH, unitsV, width, height, maxH, maxV, tables, offsets, 1);
  unsigned char p;
  for (p = 0; p < state -> component_count; p ++) state -> current_value[componentIDs[p]] = output[componentIDs[p]];
  for (; p < 4; p ++) state -> current_value[p] = NULL;
}

void initialize_JPEG_decompressor_state_common (struct context * context, struct JPEG_decompressor_state * restrict state,
                                                const struct JPEG_component_info * components, const unsigned char * componentIDs, size_t * restrict unitsH,
                                                size_t unitsV, size_t width, size_t height, unsigned char maxH, unsigned char maxV,
                                                const struct JPEG_decoder_tables * tables, const size_t * offsets, unsigned char unit_dimensions) {
  size_t p;
  if (componentIDs[1] != 0xff) {
    uint_fast8_t row, col;
    unsigned char * entry = state -> MCU;
    for (p = 0; (p < 4) && (componentIDs[p] != 0xff); p ++) {
      state -> unit_offset[componentIDs[p]] = components[componentIDs[p]].scaleH;
      state -> row_offset[componentIDs[p]] = *unitsH * state -> unit_offset[componentIDs[p]];
      state -> unit_row_offset[componentIDs[p]] = (components[componentIDs[p]].scaleV - 1) * state -> row_offset[componentIDs[p]];
      state -> row_offset[componentIDs[p]] -= state -> unit_offset[componentIDs[p]];
      for (row = 0; row < components[componentIDs[p]].scaleV; row ++) {
        *(entry ++) = row ? MCU_NEXT_ROW : MCU_ZERO_COORD;
        for (col = 0; col < components[componentIDs[p]].scaleH; col ++) *(entry ++) = componentIDs[p];
      }
    }
    *entry = MCU_END_LIST;
    state -> component_count = p;
    state -> row_skip_index = state -> row_skip_count = state -> column_skip_index = state -> column_skip_count = 0;
  } else {
    // if a scan contains a single component, it's considered a non-interleaved scan and the MCU is a single unit
    state -> component_count = 1;
    state -> unit_offset[*componentIDs] = 1;
    state -> row_offset[*componentIDs] = state -> unit_row_offset[*componentIDs] = 0;
    memcpy(state -> MCU, (unsigned char []) {MCU_ZERO_COORD, *componentIDs, MCU_END_LIST}, 3);
    *unitsH *= components[*componentIDs].scaleH;
    unitsV *= components[*componentIDs].scaleV;
    state -> column_skip_index = 1 + (width * components[*componentIDs].scaleH - 1) / (unit_dimensions * maxH);
    state -> column_skip_count = *unitsH - state -> column_skip_index;
    state -> row_skip_index = 1 + (height * components[*componentIDs].scaleV - 1) / (unit_dimensions * maxV);
    state -> row_skip_count = unitsV - state -> row_skip_index;
  }
  state -> last_size = *unitsH * unitsV;
  if (state -> restart_size = tables -> restart) {
    state -> restart_count = state -> last_size / state -> restart_size;
    state -> last_size %= state -> restart_size;
  } else
    state -> restart_count = 0;
  for (p = 0; p < state -> restart_count; p ++) if (!offsets[2 * p]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (state -> last_size && !offsets[2 * (p ++)]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (offsets[2 * p]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
}

uint16_t predict_JPEG_lossless_sample (const uint16_t * next, ptrdiff_t rowsize, int leftmost, int topmost, unsigned predictor, unsigned precision) {
  if (!predictor) return 0;
  if (topmost)
    if (leftmost)
      return 1u << (precision - 1);
    else
      return next[-1];
  else if (leftmost)
    return next[-rowsize];
  uint_fast32_t left = next[-1], top = next[-rowsize], corner = next[-1 - rowsize];
  return predictor[(const uint16_t []) {0, left, top, corner, left + top - corner, left + ((top - corner) >> 1), top + ((left - corner) >> 1), (left + top) >> 1}];
}
