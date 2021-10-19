#include "proto.h"

void load_JPEG_DCT_frame (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, size_t frameindex, 
                          struct JPEG_decoder_tables * tables, size_t * metadata_index, double ** output, unsigned precision, size_t width, size_t height) {
  const size_t * scans = layout -> framescans[frameindex];
  const size_t ** offsets = (const size_t **) layout -> framedata[frameindex];
  uint_fast8_t p, coefficient, count, maxH = 1, maxV = 1;
  struct JPEG_component_info component_info[4];
  count = get_JPEG_component_info(context, context -> data + layout -> frames[frameindex], component_info, components);
  for (p = 0; p < count; p ++) {
    if (component_info[p].scaleV > maxV) maxV = component_info[p].scaleV;
    if (component_info[p].scaleH > maxH) maxH = component_info[p].scaleH;
  }
  size_t unitrow = (width - 1) / (8 * maxH) + 1, unitcol = (height - 1) / (8 * maxV) + 1, units = unitrow * unitcol;
  int16_t (* restrict component_data[4])[64] = {0};
  for (p = 0; p < count; p ++) component_data[p] = ctxmalloc(context, sizeof **component_data * units * component_info[p].scaleH * component_info[p].scaleV);
  unsigned char currentbits[4][64];
  memset(currentbits, 0xff, sizeof currentbits);
  struct JPEG_decompressor_state state;
  for (; *scans; scans ++, offsets ++) {
    process_JPEG_metadata_until_offset(context, layout, tables, metadata_index, **offsets);
    unsigned char scancomponents[4];
    const unsigned char * progdata = get_JPEG_scan_components(context, *scans, component_info, count, scancomponents);
    if ((*progdata > progdata[1]) || (progdata[1] > 63)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint_fast8_t bitstart = progdata[2] >> 4, bitend = progdata[2] & 15;
    if ((bitstart && ((bitstart - 1) != bitend)) || (bitend > 13)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (!bitstart) bitstart = 0xff;
    for (p = 0; (p < 4) && (scancomponents[p] != 0xff); p ++) {
      if (!tables -> quantization[component_info[scancomponents[p]].tableQ]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (!(layout -> frametype[frameindex] & 8)) {
        if (!(*progdata || tables -> Huffman[component_info[scancomponents[p]].tableDC])) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (progdata[1] && !tables -> Huffman[component_info[scancomponents[p]].tableAC + 4]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      }
      for (coefficient = *progdata; coefficient <= progdata[1]; coefficient ++) {
        if (currentbits[scancomponents[p]][coefficient] != bitstart) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        currentbits[scancomponents[p]][coefficient] = bitend;
      }
    }
    size_t scanunitrow = unitrow;
    initialize_JPEG_decompressor_state(context, &state, component_info, scancomponents, &scanunitrow, unitcol, width, height, maxH, maxV, tables, *offsets,
                                       component_data);
    // call the decompression function -- they all have the same type, so select the function inline and write the argument list only once
    ((layout -> frametype[frameindex] & 8) ? ((bitstart == 0xff) ? decompress_JPEG_arithmetic_scan : decompress_JPEG_arithmetic_bit_scan) :
                                             ((bitstart == 0xff) ? decompress_JPEG_Huffman_scan : decompress_JPEG_Huffman_bit_scan))
      (context, &state, tables, scanunitrow, component_info, *offsets, bitend, *progdata, progdata[1]);
  }
  for (p = 0; p < count; p ++) for (coefficient = 0; coefficient < 64; coefficient ++)
    if (currentbits[p][coefficient]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!(layout -> frametype[frameindex] & 4)) {
    size_t i;
    double levelshift = 1u << (precision - 1);
    for (p = 0; p < count; p ++) for (i = 0; i < (width * height); i ++) output[p][i] = levelshift;
  }
  double buffer[64];
  // loop backwards so the component_data arrays are released in reverse allocation order
  while (count --) {
    size_t x, y, row, compwidth = unitrow * component_info[count].scaleH * 8 + 2, compheight = unitcol * component_info[count].scaleV * 8 + 2;
    double * transformed = ctxmalloc(context, sizeof *transformed * compwidth * compheight);
    for (y = 0; y < (unitcol * component_info[count].scaleV); y ++) for (x = 0; x < (unitrow * component_info[count].scaleH); x ++) {
      apply_JPEG_inverse_DCT(buffer, component_data[count][y * unitrow * component_info[count].scaleH + x], tables -> quantization[component_info[count].tableQ]);
      double * current = transformed + (y * 8 + 1) * compwidth + x * 8 + 1;
      for (row = 0; row < 8; row ++) memcpy(current + compwidth * row, buffer + 8 * row, sizeof *buffer * 8);
    }
    unpack_JPEG_component(output[count], transformed, width, height, compwidth, compheight, component_info[count].scaleH, component_info[count].scaleV, maxH, maxV);
    ctxfree(context, transformed);
    ctxfree(context, component_data[count]);
  }
}

unsigned load_JPEG_lossless_frame (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, size_t frameindex, 
                                   struct JPEG_decoder_tables * tables, size_t * metadata_index, double ** output, unsigned precision, size_t width, size_t height) {
  // ...
}

unsigned get_JPEG_component_info (struct context * context, const unsigned char * frameheader, struct JPEG_component_info * restrict output, uint32_t components) {
  // assumes the component list is correct - true by definition for single-frame images and checked elsewhere for hierarchical ones
  unsigned char component_numbers[4];
  write_le32_unaligned(component_numbers, components);
  uint_fast8_t p, index, current, count = get_JPEG_component_count(components);
  for (current = 0; current < count; current ++) {
    for (index = 0; index < count; index ++) if (component_numbers[index] == frameheader[8 + 3 * current]) break;
    output[index].index = component_numbers[index];
    p = frameheader[9 + 3 * current];
    output[index].scaleH = p >> 4;
    output[index].scaleV = p & 15;
    output[index].tableQ = frameheader[10 + 3 * current];
    if (!output[index].scaleH || (output[index].scaleH > 4) || !output[index].scaleV || (output[index].scaleV > 4) || (output[index].tableQ > 3))
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  return count;
}

const unsigned char * get_JPEG_scan_components (struct context * context, size_t offset, struct JPEG_component_info * restrict compinfo, unsigned framecount,
                                                unsigned char * restrict output) {
  uint_fast16_t headerlength = read_be16_unaligned(context -> data + offset);
  if (headerlength < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast8_t p, index, count = context -> data[offset + 2];
  if (headerlength != (6 + 2 * count)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  memset(output, -1, 4);
  for (p = 0; p < count; p ++) {
    for (index = 0; index < framecount; index ++) if (compinfo[index].index == context -> data[offset + 3 + 2 * p]) break;
    if (index == framecount) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    output[p] = index;
    compinfo[index].tableDC = context -> data[offset + 4 + 2 * p] >> 4;
    compinfo[index].tableAC = context -> data[offset + 4 + 2 * p] & 15;
    if ((compinfo[index].tableDC > 3) || (compinfo[index].tableAC > 3)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  return context -> data + offset + 3 + 2 * count;
}

void initialize_JPEG_decompressor_state (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_component_info * components,
                                         const unsigned char * componentIDs, size_t * restrict unitsH, size_t unitsV, size_t width, size_t height,
                                         unsigned char maxH, unsigned char maxV, const struct JPEG_decoder_tables * tables, const size_t * offsets,
                                         int16_t (* restrict * output)[64]) {
  size_t p;
  for (p = 0; p < 4; p ++) state -> current[p] = NULL;
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
      state -> current[componentIDs[p]] = output[componentIDs[p]];
    }
    *entry = MCU_END_LIST;
    state -> component_count = p;
    state -> row_skip_index = state -> row_skip_count = state -> column_skip_index = state -> column_skip_count = 0;
  } else {
    // if a scan contains a single component, it's considered a non-interleaved scan and the MCU is a single 8x8 block
    state -> component_count = 1;
    state -> unit_offset[*componentIDs] = 1;
    state -> row_offset[*componentIDs] = state -> unit_row_offset[*componentIDs] = 0;
    memcpy(state -> MCU, (unsigned char []) {MCU_ZERO_COORD, *componentIDs, MCU_END_LIST}, 3);
    *unitsH *= components[*componentIDs].scaleH;
    unitsV *= components[*componentIDs].scaleV;
    state -> current[*componentIDs] = output[*componentIDs];
    state -> column_skip_index = 1 + (width * components[*componentIDs].scaleH - 1) / (8 * maxH);
    state -> column_skip_count = *unitsH - state -> column_skip_index;
    state -> row_skip_index = 1 + (height * components[*componentIDs].scaleV - 1) / (8 * maxV);
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

void unpack_JPEG_component (double * restrict result, double * restrict source, size_t width, size_t height, size_t scaled_width, size_t scaled_height,
                            unsigned char scaleH, unsigned char scaleV, unsigned char maxH, unsigned char maxV) {
  // takes in a source that has a one-cell border around the data, fills it in with duplicated data, interpolates the data and adds it to the output
  size_t p, scaled_size = scaled_width * scaled_height;
  for (p = 1; p < (scaled_width - 1); p ++) source[p] = source[p + scaled_width];
  for (p = 2; p < scaled_width; p ++) source[scaled_size - p] = source[scaled_size - p - scaled_width];
  for (p = 0; p < scaled_height; p ++) {
    source[p * scaled_width] = source[p * scaled_width + 1];
    source[(p + 1) * scaled_width - 1] = source[(p + 1) * scaled_width - 2];
  }
  if (scaleH == maxH) scaleH = maxH = 1;
  if (scaleV == maxV) scaleV = maxV = 1;
  if ((maxH == 4) && (scaleH == 2)) {
    maxH = 2;
    scaleH = 1;
  }
  if ((maxV == 4) && (scaleV == 2)) {
    maxV = 2;
    scaleV = 1;
  }
  // indexes into the interpolation index lists: 1-4 for integer ratios (scale = 1 after normalization above), 0 for 3/2, 5 for 4/3
  unsigned char indexH = (scaleH != 1) ? (scaleH - 2) * 5 : maxH;
  unsigned char indexV = (scaleV != 1) ? (scaleV - 2) * 5 : maxV;
  // weights for all possible scaling factors (3/2, 1 to 4, 4/3); a subset of these will be selected for each axis depending on the actual scale factor
  static const double interpolation_weights[] = {0x0.55555555555558p+0, 0x0.aaaaaaaaaaaaa8p+0, 1.0, 0x0.aaaaaaaaaaaaa8p+0, 0x0.55555555555558p+0, 0.0,
                                                 0x0.4p+0, 0x0.cp+0, 0x0.4p+0, 0x0.2aaaaaaaaaaaa8p+0, 0x0.8p+0, 0x0.d5555555555558p+0, 0x0.8p+0,
                                                 0x0.2aaaaaaaaaaaa8p+0, 0x0.2p+0, 0x0.6p+0, 0x0.ap+0, 0x0.ep+0, 0x0.ap+0, 0x0.6p+0, 0x0.2p+0};
  const double * firstH = interpolation_weights + indexH[(const size_t []) {9, 5, 7, 3, 17, 14}];
  const double * firstV = interpolation_weights + indexV[(const size_t []) {9, 5, 7, 3, 17, 14}];
  const double * secondH = interpolation_weights + indexH[(const size_t []) {11, 2, 6, 0, 14, 17}];
  const double * secondV = interpolation_weights + indexV[(const size_t []) {11, 2, 6, 0, 14, 17}];
  unsigned char offsetH, offsetV = maxV / (2 * scaleV);
  size_t row, col, sourceX, sourceY = 0;
  p = 0;
  for (row = 0; row < height; row ++) {
    sourceX = 0;
    offsetH = maxH / (2 * scaleH);
    for (col = 0; col < width; col ++) {
      result[p ++] += source[sourceX + sourceY * scaled_width] * firstH[offsetH] * firstV[offsetV] +
                      source[sourceX + 1 + sourceY * scaled_width] * secondH[offsetH] * firstV[offsetV] +
                      source[sourceX + (sourceY + 1) * scaled_width] * firstH[offsetH] * secondV[offsetV] +
                      source[sourceX + 1 + (sourceY + 1) * scaled_width] * secondH[offsetH] * secondV[offsetV];
      if ((++ offsetH) == maxH) {
        offsetH = 0;
        if (scaleH == 1) sourceX ++;
      } else if (scaleH != 1)
        sourceX ++;
    }
    if ((++ offsetV) == maxV) {
      offsetV = 0;
      if (scaleV == 1) sourceY ++;
    } else if (scaleV != 1)
      sourceY ++;
  }
}
