#include "proto.h"

void load_JPEG_DCT_frame (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, size_t frameindex,
                          struct JPEG_decoder_tables * tables, size_t * metadata_index, double ** output, unsigned precision, size_t width, size_t height) {
  const size_t * scans = layout -> framescans[frameindex];
  const size_t ** offsets = (const size_t **) layout -> framedata[frameindex];
  // obtain this frame's components' parameters and compute the number of (non-subsampled) blocks per MCU (maximum scale factor for each dimension)
  struct JPEG_component_info component_info[4];
  uint_fast8_t maxH = 1, maxV = 1, count = get_JPEG_component_info(context, context -> data + layout -> frames[frameindex], component_info, components);
  for (uint_fast8_t p = 0; p < count; p ++) {
    if (component_info[p].scaleV > maxV) maxV = component_info[p].scaleV;
    if (component_info[p].scaleH > maxH) maxH = component_info[p].scaleH;
  }
  // compute the image dimensions in MCUs and allocate space for that many coefficients for each component (including padding blocks to fill up edge MCUs)
  size_t unitrow = (width - 1) / (8 * maxH) + 1, unitcol = (height - 1) / (8 * maxV) + 1, units = unitrow * unitcol;
  int16_t (* restrict component_data[4])[64] = {0};
  for (uint_fast8_t p = 0; p < count; p ++)
    component_data[p] = ctxmalloc(context, sizeof **component_data * units * component_info[p].scaleH * component_info[p].scaleV);
  unsigned char currentbits[4][64]; // successive approximation bit positions for each component and coefficient, for progressive scans
  memset(currentbits, 0xff, sizeof currentbits); // 0xff = no data yet (i.e., the coefficient hasn't shown up yet in any scans)
  for (; *scans; scans ++, offsets ++) {
    if (process_JPEG_metadata_until_offset(context, layout, tables, metadata_index, **offsets)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    unsigned char scancomponents[4];
    const unsigned char * progdata = get_JPEG_scan_components(context, *scans, component_info, count, scancomponents);
    // validate the spectral selection parameters
    uint_fast8_t first = *progdata, last = progdata[1];
    if (first > last || last > 63) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    // validate and update the successive approximation bit positions for each component and coefficient involved in the scan
    uint_fast8_t bitstart = progdata[2] >> 4, bitend = progdata[2] & 15;
    if ((bitstart && bitstart - 1 != bitend) || bitend > 13 || bitend >= precision) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (!bitstart) bitstart = 0xff; // 0xff = not a progressive scan (intentionally matches the "no data yet" 0xff in currentbits)
    for (uint_fast8_t p = 0; p < 4 && scancomponents[p] != 0xff; p ++) {
      // check that all quantization and Huffman tables (when applicable) used by the scan have already been loaded
      if (!tables -> quantization[component_info[scancomponents[p]].tableQ]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (!(layout -> frametype[frameindex] & 8)) {
        if (!(first || tables -> Huffman[component_info[scancomponents[p]].tableDC])) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (last && !tables -> Huffman[component_info[scancomponents[p]].tableAC + 4]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      }
      // ensure that this scan's successive approximation bit parameters match what is expected based on previous scans, and update currentbits
      for (uint_fast8_t coefficient = first; coefficient <= last; coefficient ++) {
        if (currentbits[scancomponents[p]][coefficient] != bitstart) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        currentbits[scancomponents[p]][coefficient] = bitend;
      }
    }
    size_t scanunitrow = unitrow;
    struct JPEG_decompressor_state state;
    initialize_JPEG_decompressor_state(context, &state, component_info, scancomponents, &scanunitrow, unitcol, width, height, maxH, maxV, tables, *offsets,
                                       component_data);
    // call the decompression function, depending on the frame type (Huffman or arithmetic) and whether it is progressive or not
    if (bitstart == 0xff)
      if (layout -> frametype[frameindex] & 8)
        decompress_JPEG_arithmetic_scan(context, &state, tables, scanunitrow, component_info, *offsets, bitend, first, last, layout -> frametype[frameindex] & 4);
      else
        decompress_JPEG_Huffman_scan(context, &state, tables, scanunitrow, component_info, *offsets, bitend, first, last, layout -> frametype[frameindex] & 4);
    else
      if (layout -> frametype[frameindex] & 8)
        decompress_JPEG_arithmetic_bit_scan(context, &state, scanunitrow, component_info, *offsets, bitend, first, last);
      else
        decompress_JPEG_Huffman_bit_scan(context, &state, tables, scanunitrow, component_info, *offsets, bitend, first, last);
  }
  // ensure that the frame's scans contain all bits for all coefficients, for each one of its components
  for (uint_fast8_t p = 0; p < count; p ++) for (uint_fast8_t coefficient = 0; coefficient < 64; coefficient ++)
    if (currentbits[p][coefficient]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // if the frame is non-differential, initialize all components in the final image to the level shift value
  if (!(layout -> frametype[frameindex] & 4)) {
    double levelshift = 1u << (precision - 1);
    for (uint_fast8_t p = 0; p < count; p ++) for (size_t i = 0; i < width * height; i ++) output[p][i] = levelshift;
  }
  // transform all blocks into component data and add it to the output (level shift value for non-differential frames, previous values for differential frames)
  // loop backwards so DCT data is released in reverse allocation order after transforming it into output data
  while (count --) {
    size_t compwidth = unitrow * component_info[count].scaleH * 8 + 2, compheight = unitcol * component_info[count].scaleV * 8 + 2;
    double * transformed = ctxmalloc(context, sizeof *transformed * compwidth * compheight); // component data buffer, plus a pixel of padding around the edges
    for (size_t y = 0; y < unitcol * component_info[count].scaleV; y ++) for (size_t x = 0; x < unitrow * component_info[count].scaleH; x ++) {
      // apply the reverse DCT to each block, transforming it into component data
      double buffer[64];
      apply_JPEG_inverse_DCT(buffer, component_data[count][y * unitrow * component_info[count].scaleH + x], tables -> quantization[component_info[count].tableQ]);
      // copy the block's data to the correct location in the component data buffer (accounting for the padding)
      double * current = transformed + (y * 8 + 1) * compwidth + x * 8 + 1;
      for (uint_fast8_t row = 0; row < 8; row ++) memcpy(current + compwidth * row, buffer + 8 * row, sizeof *buffer * 8);
    }
    // scale up subsampled components and add them to the output
    unpack_JPEG_component(output[count], transformed, width, height, compwidth, compheight, component_info[count].scaleH, component_info[count].scaleV, maxH, maxV);
    ctxfree(context, transformed);
    ctxfree(context, component_data[count]);
  }
}

void load_JPEG_lossless_frame (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, size_t frameindex,
                               struct JPEG_decoder_tables * tables, size_t * metadata_index, double ** output, unsigned precision, size_t width, size_t height) {
  const size_t * scans = layout -> framescans[frameindex];
  const size_t ** offsets = (const size_t **) layout -> framedata[frameindex];
  // obtain this frame's components' parameters and compute the number of pixels per MCU (maximum scale factor for each dimension)
  struct JPEG_component_info component_info[4];
  uint_fast8_t maxH = 1, maxV = 1, count = get_JPEG_component_info(context, context -> data + layout -> frames[frameindex], component_info, components);
  for (uint_fast8_t p = 0; p < count; p ++) {
    if (component_info[p].scaleV > maxV) maxV = component_info[p].scaleV;
    if (component_info[p].scaleH > maxH) maxH = component_info[p].scaleH;
  }
  // compute the image dimensions in MCUs and allocate space for that many pixels for each component (including padding pixels to fill edge MCUs)
  size_t unitrow = (width - 1) / maxH + 1, unitcol = (height - 1) / maxV + 1, units = unitrow * unitcol;
  uint16_t * restrict component_data[4] = {0};
  for (uint_fast8_t p = 0; p < count; p ++)
    component_data[p] = ctxmalloc(context, sizeof **component_data * units * component_info[p].scaleH * component_info[p].scaleV);
  double initial_value[4]; // offset to add to pixel data, to reduce rounding errors in shifted-down components (0 if no offset is needed)
  int component_shift[4] = {-1, -1, -1, -1}; // shift amounts for each component (negative: the component hasn't shown up in any scans yet)
  for (; *scans; scans ++, offsets ++) {
    if (process_JPEG_metadata_until_offset(context, layout, tables, metadata_index, **offsets)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    unsigned char scancomponents[4];
    const unsigned char * progdata = get_JPEG_scan_components(context, *scans, component_info, count, scancomponents);
    uint_fast8_t predictor = *progdata, shift = progdata[2] & 15;
    if (predictor > 7 || shift >= precision) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    for (uint_fast8_t p = 0; p < 4 && scancomponents[p] != 0xff; p ++) {
      // check that the components from this scan haven't already been decoded, and update the components' shift amount and initial value
      if (component_shift[scancomponents[p]] >= 0) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      component_shift[scancomponents[p]] = shift;
      if (layout -> hierarchical)
        initial_value[scancomponents[p]] = 0;
      else
        initial_value[scancomponents[p]] = shift ? 1u << (shift - 1) : 0;
      // if the frame is a Huffman frame, ensure that all Huffman tables used by the scan have already been loaded
      if (!((layout -> frametype[frameindex] & 8) || tables -> Huffman[component_info[scancomponents[p]].tableDC])) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    size_t scanunitrow = unitrow;
    struct JPEG_decompressor_state state;
    initialize_JPEG_decompressor_state_lossless(context, &state, component_info, scancomponents, &scanunitrow, unitcol, width, height, maxH, maxV, tables,
                                                *offsets, component_data);
    // call the decompression function, depending on the frame type (Huffman or arithmetic) - lossless scans cannot be progressive
    if (layout -> frametype[frameindex] & 8)
      decompress_JPEG_arithmetic_lossless_scan(context, &state, tables, scanunitrow, component_info, *offsets, predictor, precision - shift);
    else
      decompress_JPEG_Huffman_lossless_scan(context, &state, tables, scanunitrow, component_info, *offsets, predictor, precision - shift);
  }
  // ensure that all components in the frame have appeared in some scan
  for (uint_fast8_t p = 0; p < count; p ++) if (component_shift[p] < 0) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // convert all decoded component data into actual component pixels; loop backwards so that temporary component data is released in reverse allocation order
  while (count --) {
    size_t compwidth = unitrow * component_info[count].scaleH + 2, compheight = unitcol * component_info[count].scaleV + 2;
    double * converted = ctxmalloc(context, sizeof *converted * compwidth * compheight); // output data buffer, plus a pixel of padding around the edges
    // shift up all component pixels as needed, and store them in the correct location in the output data buffer (accounting for the padding)
    for (size_t y = 0; y < unitcol * component_info[count].scaleV; y ++) for (size_t x = 0; x < unitrow * component_info[count].scaleH; x ++)
      converted[(y + 1) * compwidth + x + 1] = component_data[count][y * unitrow * component_info[count].scaleH + x] << component_shift[count];
    // add the initial value to all component values and scale up subsampled components as needed
    if (!(layout -> frametype[frameindex] & 4)) for (size_t p = 0; p < width * height; p ++) output[count][p] = initial_value[count];
    unpack_JPEG_component(output[count], converted, width, height, compwidth, compheight, component_info[count].scaleH, component_info[count].scaleV, maxH, maxV);
    ctxfree(context, converted);
    ctxfree(context, component_data[count]);
  }
}

unsigned get_JPEG_component_info (struct context * context, const unsigned char * frameheader, struct JPEG_component_info * restrict output, uint32_t components) {
  // assumes the component list is correct - true by definition for single-frame images and checked elsewhere for hierarchical ones
  unsigned char component_numbers[4];
  write_le32_unaligned(component_numbers, components);
  uint_fast8_t count = get_JPEG_component_count(components);
  for (uint_fast8_t current = 0; current < count; current ++) {
    uint_fast8_t index;
    for (index = 0; index < count; index ++) if (component_numbers[index] == frameheader[8 + 3 * current]) break;
    output[index].index = component_numbers[index];
    uint_fast8_t p = frameheader[9 + 3 * current];
    output[index].scaleH = p >> 4;
    output[index].scaleV = p & 15;
    output[index].tableQ = frameheader[10 + 3 * current];
    if (!output[index].scaleH || output[index].scaleH > 4 || !output[index].scaleV || output[index].scaleV > 4 || output[index].tableQ > 3)
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  return count;
}

const unsigned char * get_JPEG_scan_components (struct context * context, size_t offset, struct JPEG_component_info * restrict compinfo, unsigned framecount,
                                                unsigned char * restrict output) {
  uint_fast16_t headerlength = read_be16_unaligned(context -> data + offset);
  if (headerlength < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast8_t count = context -> data[offset + 2];
  if (headerlength != 6 + 2 * count) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  memset(output, 0xff, 4);
  for (uint_fast8_t p = 0; p < count; p ++) {
    uint_fast8_t index;
    for (index = 0; index < framecount; index ++) if (compinfo[index].index == context -> data[offset + 3 + 2 * p]) break;
    if (index == framecount) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    output[p] = index;
    compinfo[index].tableDC = context -> data[offset + 4 + 2 * p] >> 4;
    compinfo[index].tableAC = context -> data[offset + 4 + 2 * p] & 15;
    if (compinfo[index].tableDC > 3 || compinfo[index].tableAC > 3) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  return context -> data + offset + 3 + 2 * count;
}

void unpack_JPEG_component (double * restrict result, double * restrict source, size_t width, size_t height, size_t scaled_width, size_t scaled_height,
                            unsigned char scaleH, unsigned char scaleV, unsigned char maxH, unsigned char maxV) {
  // fill the border of the component data (one pixel of dummy values around the edges) by copying values from the true edges
  size_t scaled_size = scaled_width * scaled_height;
  for (size_t p = 1; p < scaled_width - 1; p ++) source[p] = source[p + scaled_width];
  for (size_t p = 2; p < scaled_width; p ++) source[scaled_size - p] = source[scaled_size - p - scaled_width];
  for (size_t p = 0; p < scaled_height; p ++) {
    source[p * scaled_width] = source[p * scaled_width + 1];
    source[(p + 1) * scaled_width - 1] = source[(p + 1) * scaled_width - 2];
  }
  // if the scaling parameters form a reducible fraction, reduce it
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
  unsigned char indexH = (scaleH != 1) ? (scaleH == 3) * 5 : maxH;
  unsigned char indexV = (scaleV != 1) ? (scaleV == 3) * 5 : maxV;
  // weights for all possible scaling factors (3/2, 1 to 4, 4/3); a subset of these will be selected for each axis depending on the actual scale factor
  static const double interpolation_weights[] = {0x0.55555555555558p+0, 0x0.aaaaaaaaaaaaa8p+0, 1.0, 0x0.aaaaaaaaaaaaa8p+0, 0x0.55555555555558p+0, 0.0,
                                                 0x0.4p+0, 0x0.cp+0, 0x0.4p+0, 0x0.2aaaaaaaaaaaa8p+0, 0x0.8p+0, 0x0.d5555555555558p+0, 0x0.8p+0,
                                                 0x0.2aaaaaaaaaaaa8p+0, 0x0.2p+0, 0x0.6p+0, 0x0.ap+0, 0x0.ep+0, 0x0.ap+0, 0x0.6p+0, 0x0.2p+0};
  static const unsigned char first_interpolation_indexes[] = {9, 5, 7, 3, 17, 14};
  static const unsigned char second_interpolation_indexes[] = {11, 2, 6, 0, 14, 17};
  // actual interpolation weights for each pixel in a row/column of upscaled pixels
  const double * firstH = interpolation_weights + first_interpolation_indexes[indexH];
  const double * firstV = interpolation_weights + first_interpolation_indexes[indexV];
  const double * secondH = interpolation_weights + second_interpolation_indexes[indexH];
  const double * secondV = interpolation_weights + second_interpolation_indexes[indexV];
  // scale up the component, as determined by the scale parameters, by interpolating the decoded data
  unsigned char offsetV = maxV / (2 * scaleV);
  size_t p = 0, sourceY = 0;
  for (size_t row = 0; row < height; row ++) {
    size_t sourceX = 0;
    unsigned char offsetH = maxH / (2 * scaleH);
    for (size_t col = 0; col < width; col ++) {
      result[p ++] += source[sourceX + sourceY * scaled_width] * firstH[offsetH] * firstV[offsetV] +
                      source[sourceX + 1 + sourceY * scaled_width] * secondH[offsetH] * firstV[offsetV] +
                      source[sourceX + (sourceY + 1) * scaled_width] * firstH[offsetH] * secondV[offsetV] +
                      source[sourceX + 1 + (sourceY + 1) * scaled_width] * secondH[offsetH] * secondV[offsetV];
      if (++ offsetH == maxH) {
        offsetH = 0;
        if (scaleH == 1) sourceX ++;
      } else if (scaleH != 1)
        sourceX ++;
    }
    if (++ offsetV == maxV) {
      offsetV = 0;
      if (scaleV == 1) sourceY ++;
    } else if (scaleV != 1)
      sourceY ++;
  }
}
