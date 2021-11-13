#include "proto.h"

void decompress_JPEG_arithmetic_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                      size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                      unsigned char last) {
  size_t restart_interval;
  for (restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    int16_t (* outputunit)[64];
    const unsigned char * decodepos;
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t accumulator = 0;
    uint32_t current = 0;
    unsigned char p, conditioning, bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexesDC[4][49] = {0};
    signed char indexesAC[4][245] = {0};
    uint16_t prevDC[4] = {0};
    uint16_t prevdiff[4] = {0};
    int prevzero;
    while (units --) {
      for (decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputunit = state -> current_block[decodepos[1]];
          break;
        case MCU_NEXT_ROW:
          outputunit += state -> row_offset[decodepos[1]];
          break;
        default:
          prevzero = 0;
          for (p = first; p <= last; p ++) {
            if (skipunits)
              p[*outputunit] = 0;
            else if (p) {
              conditioning = tables -> arithmetic[components[*decodepos].tableAC + 4];
              signed char * index = indexesAC[components[*decodepos].tableAC] + 3 * (p - 1);
              if (!prevzero && next_JPEG_arithmetic_bit(context, &offset, &remaining, index, &current, &accumulator, &bits)) {
                p[*outputunit] = 0;
                skipunits ++;
              } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 1, &current, &accumulator, &bits)) {
                p[*outputunit] = next_JPEG_arithmetic_value(context, &offset, &remaining, &current, &accumulator, &bits, indexesAC[components[*decodepos].tableAC],
                                                            1, p, conditioning);
                prevzero = 0;
              } else {
                p[*outputunit] = 0;
                prevzero = 1;
              }
            } else {
              conditioning = tables -> arithmetic[components[*decodepos].tableDC];
              unsigned char category = classify_JPEG_arithmetic_value(prevdiff[*decodepos], conditioning);
              if (next_JPEG_arithmetic_bit(context, &offset, &remaining, indexesDC[components[*decodepos].tableDC] + 4 * category, &current, &accumulator, &bits))
                prevdiff[*decodepos] = next_JPEG_arithmetic_value(context, &offset, &remaining, &current, &accumulator, &bits,
                                                          indexesDC[components[*decodepos].tableDC], 0, category, conditioning);
              else
                prevdiff[*decodepos] = 0;
              prevDC[*decodepos] = **outputunit = make_signed_16(prevDC[*decodepos] + prevdiff[*decodepos]);
            }
            p[*outputunit] = make_signed_16((uint16_t) p[*outputunit] << shift);
          }
          outputunit ++;
          if (skipunits) skipunits --;
      }
      if ((++ colcount) == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (p = 0; p < 4; p ++) if (state -> current_block[p]) {
        state -> current_block[p] += state -> unit_offset[p];
        if (!colcount) state -> current_block[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_arithmetic_bit_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                          size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift,
                                          unsigned char first, unsigned char last) {
  // this function is very similar to decompress_JPEG_arithmetic_scan, but it only decodes the next bit for already-initialized data
  if (last && !first) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t restart_interval;
  for (restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    int16_t (* outputunit)[64];
    const unsigned char * decodepos;
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t accumulator = 0;
    uint32_t current = 0;
    unsigned char p, conditioning, bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexes[4][189] = {0}; // most likely very few will be actually used, but allocate for the worst case
    while (units --) {
      for (decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputunit = state -> current_block[decodepos[1]];
          break;
        case MCU_NEXT_ROW:
          outputunit += state -> row_offset[decodepos[1]];
          break;
        default:
          if (skipunits)
            skipunits --;
          else if (first) {
            unsigned char lastnonzero;
            for (lastnonzero = 63; lastnonzero; lastnonzero --) if (lastnonzero[*outputunit]) break;
            int prevzero = 0;
            conditioning = tables -> arithmetic[components[*decodepos].tableAC + 4];
            for (p = first; p <= last; p ++) {
              signed char * index = indexes[components[*decodepos].tableAC] + 3 * (p - 1);
              if (!prevzero && (p > lastnonzero) && next_JPEG_arithmetic_bit(context, &offset, &remaining, index, &current, &accumulator, &bits)) break;
              if (p[*outputunit]) {
                prevzero = 0;
                if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 2, &current, &accumulator, &bits))
                  if (p[*outputunit] < 0)
                    p[*outputunit] -= 1 << shift;
                  else
                    p[*outputunit] += 1 << shift;
              } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 1, &current, &accumulator, &bits)) {
                prevzero = 0;
                p[*outputunit] = next_JPEG_arithmetic_bit(context, &offset, &remaining, NULL, &current, &accumulator, &bits) ? -1 << shift : (1 << shift);
              } else
                prevzero = 1;
            }
          } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, NULL, &current, &accumulator, &bits))
            **outputunit += 1 << shift;
          outputunit ++;
      }
      if ((++ colcount) == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (p = 0; p < 4; p ++) if (state -> current_block[p]) {
        state -> current_block[p] += state -> unit_offset[p];
        if (!colcount) state -> current_block[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_arithmetic_lossless_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                               size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift,
                                               unsigned char predictor, unsigned precision) {
  size_t p, restart_interval;
  uint16_t * rowdifferences[4] = {0};
  for (p = 0; p < state -> component_count; p ++) rowdifferences[p] = ctxmalloc(context, sizeof **rowdifferences * rowunits * components[p].scaleH);
  for (restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    uint16_t * outputpos;
    const unsigned char * decodepos;
    size_t x, y, colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t predicted, difference, accumulator = 0;
    uint32_t current = 0;
    unsigned char conditioning, bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexes[4][158] = {0};
    for (p = 0; p < state -> component_count; p ++) for (x = 0; x < (rowunits * components[p].scaleH); x ++) rowdifferences[p][x] = 0;
    uint16_t coldifferences[4][4] = {0};
    while (units --) {
      for (decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputpos = state -> current_value[decodepos[1]];
          x = colcount * components[decodepos[1]].scaleH;
          y = 0;
          break;
        case MCU_NEXT_ROW:
          outputpos += state -> row_offset[decodepos[1]];
          x = colcount * components[decodepos[1]].scaleH;
          y ++;
          break;
        default:
          if (skipunits) {
            *(outputpos ++) = 0;
            skipunits --;
          } else {
            conditioning = tables -> arithmetic[components[*decodepos].tableDC];
            predicted = predict_JPEG_lossless_sample(outputpos, rowunits, rowcount, colcount, predictor, precision);
            // the JPEG standard calculates this the other way around, but it makes no difference and doing it in this order enables an optimization
            unsigned char reference = 5 * classify_JPEG_arithmetic_value(rowdifferences[*decodepos][x], conditioning) +
                                      classify_JPEG_arithmetic_value(coldifferences[*decodepos][y], conditioning);
            if (next_JPEG_arithmetic_bit(context, &offset, &remaining, indexes[components[*decodepos].tableDC] + 4 * reference, &current, &accumulator, &bits))
              difference = next_JPEG_arithmetic_value(context, &offset, &remaining, &current, &accumulator, &bits, indexes[components[*decodepos].tableDC],
                                                      2, reference, conditioning);
            else
              difference = 0;
            rowdifferences[*decodepos][x] = coldifferences[*decodepos][y] = difference;
            *(outputpos ++) = predicted + difference;
          }
          x ++;
      }
      if ((++ colcount) == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
        memset(coldifferences, 0, sizeof coldifferences);
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (p = 0; p < 4; p ++) if (state -> current_value[p]) {
        state -> current_value[p] += state -> unit_offset[p];
        if (!colcount) state -> current_value[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  for (p = 0; p < state -> component_count; p ++) ctxfree(context, rowdifferences[p]);
}

void initialize_JPEG_arithmetic_counters (struct context * context, size_t * restrict offset, size_t * restrict remaining, uint32_t * restrict current) {
  unsigned char data, loopcount = 2;
  while (loopcount --) {
    data = 0;
    if (*remaining) {
      data = context -> data[(*offset) ++];
      -- *remaining;
    }
    if (data == 0xff) while (*remaining) {
      -- *remaining;
      if (context -> data[(*offset) ++] != 0xff) break;
    }
    *current = (*current | data) << 8;
  }
}

int16_t next_JPEG_arithmetic_value (struct context * context, size_t * restrict offset, size_t * restrict remaining, uint32_t * restrict current,
                                    uint16_t * restrict accumulator, unsigned char * restrict bits, signed char * restrict indexes, int mode, unsigned reference,
                                    unsigned char conditioning) {
  // mode = 0 for DC (reference = DC category), 1 for AC (reference = coefficient index), 2 for lossless (reference = 5 * top category + left category)
  signed char * index = (mode == 1) ? NULL : (indexes + 4 * reference + 1);
  unsigned size, negative = next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits);
  index = (mode == 1) ? indexes + 3 * reference - 1 : (index + 1 + negative);
  size = next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits);
  uint16_t result = 0;
  if (size) {
    if (!mode)
      index = indexes + 20;
    else if (mode == 2)
      index = indexes + 100 + 29 * (reference >= 15);
    signed char * next_index = (mode == 1) ? indexes + 189 + 28 * (reference > conditioning) : (index + 1);
    while (next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits)) {
      size ++;
      if (size > 15) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      index = next_index ++;
    }
    result = 1;
    index += 14;
    while (-- size) result = (result << 1) + next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits);
  }
  result ++;
  if (negative) result = -result;
  return make_signed_16(result);
}

unsigned char classify_JPEG_arithmetic_value (uint16_t value, unsigned char conditioning) {
  // 0-4 for zero, small positive, small negative, large positive, large negative
  uint16_t absolute = (value >= 0x8000u) ? -value : value;
  uint16_t low = 0, high = (uint16_t) 1 << (conditioning >> 4);
  conditioning &= 15;
  if (conditioning) low = 1 << (conditioning - 1);
  if (absolute <= low) return 0;
  return ((value >= 0x8000u) ? 2 : 1) + 2 * (absolute > high);
}

unsigned next_JPEG_arithmetic_bit (struct context * context, size_t * restrict offset, size_t * restrict remaining, signed char * restrict index,
                                   uint32_t * restrict current, uint16_t * restrict accumulator, unsigned char * restrict bits) {
  // negative state index: MPS = 1; null state: use 0 and don't update
  // index 0 implies MPS = 0; there's no way to encode index = 0 and MPS = 1 (because that'd be state = -0), but that state cannot happen
  static const struct JPEG_arithmetic_decoder_state states[] = {
    /*   0 */ {0x5a1d, 1,   1,   1}, {0x2586, 0,   2,  14}, {0x1114, 0,   3,  16}, {0x080b, 0,   4,  18}, {0x03d8, 0,   5,  20},
    /*   5 */ {0x01da, 0,   6,  23}, {0x00e5, 0,   7,  25}, {0x006f, 0,   8,  28}, {0x0036, 0,   9,  30}, {0x001a, 0,  10,  33},
    /*  10 */ {0x000d, 0,  11,  35}, {0x0006, 0,  12,   9}, {0x0003, 0,  13,  10}, {0x0001, 0,  13,  12}, {0x5a7f, 1,  15,  15},
    /*  15 */ {0x3f25, 0,  16,  36}, {0x2cf2, 0,  17,  38}, {0x207c, 0,  18,  39}, {0x17b9, 0,  19,  40}, {0x1182, 0,  20,  42},
    /*  20 */ {0x0cef, 0,  21,  43}, {0x09a1, 0,  22,  45}, {0x072f, 0,  23,  46}, {0x055c, 0,  24,  48}, {0x0406, 0,  25,  49},
    /*  25 */ {0x0303, 0,  26,  51}, {0x0240, 0,  27,  52}, {0x01b1, 0,  28,  54}, {0x0144, 0,  29,  56}, {0x00f5, 0,  30,  57},
    /*  30 */ {0x00b7, 0,  31,  59}, {0x008a, 0,  32,  60}, {0x0068, 0,  33,  62}, {0x004e, 0,  34,  63}, {0x003b, 0,  35,  32},
    /*  35 */ {0x002c, 0,   9,  33}, {0x5ae1, 1,  37,  37}, {0x484c, 0,  38,  64}, {0x3a0d, 0,  39,  65}, {0x2ef1, 0,  40,  67},
    /*  40 */ {0x261f, 0,  41,  68}, {0x1f33, 0,  42,  69}, {0x19a8, 0,  43,  70}, {0x1518, 0,  44,  72}, {0x1177, 0,  45,  73},
    /*  45 */ {0x0e74, 0,  46,  74}, {0x0bfb, 0,  47,  75}, {0x09f8, 0,  48,  77}, {0x0861, 0,  49,  78}, {0x0706, 0,  50,  79},
    /*  50 */ {0x05cd, 0,  51,  48}, {0x04de, 0,  52,  50}, {0x040f, 0,  53,  50}, {0x0363, 0,  54,  51}, {0x02d4, 0,  55,  52},
    /*  55 */ {0x025c, 0,  56,  53}, {0x01f8, 0,  57,  54}, {0x01a4, 0,  58,  55}, {0x0160, 0,  59,  56}, {0x0125, 0,  60,  57},
    /*  60 */ {0x00f6, 0,  61,  58}, {0x00cb, 0,  62,  59}, {0x00ab, 0,  63,  61}, {0x008f, 0,  32,  61}, {0x5b12, 1,  65,  65},
    /*  65 */ {0x4d04, 0,  66,  80}, {0x412c, 0,  67,  81}, {0x37d8, 0,  68,  82}, {0x2fe8, 0,  69,  83}, {0x293c, 0,  70,  84},
    /*  70 */ {0x2379, 0,  71,  86}, {0x1edf, 0,  72,  87}, {0x1aa9, 0,  73,  87}, {0x174e, 0,  74,  72}, {0x1424, 0,  75,  72},
    /*  75 */ {0x119c, 0,  76,  74}, {0x0f6b, 0,  77,  74}, {0x0d51, 0,  78,  75}, {0x0bb6, 0,  79,  77}, {0x0a40, 0,  48,  77},
    /*  80 */ {0x5832, 1,  81,  80}, {0x4d1c, 0,  82,  88}, {0x438e, 0,  83,  89}, {0x3bdd, 0,  84,  90}, {0x34ee, 0,  85,  91},
    /*  85 */ {0x2eae, 0,  86,  92}, {0x299a, 0,  87,  93}, {0x2516, 0,  71,  86}, {0x5570, 1,  89,  88}, {0x4ca9, 0,  90,  95},
    /*  90 */ {0x44d9, 0,  91,  96}, {0x3e22, 0,  92,  97}, {0x3824, 0,  93,  99}, {0x32b4, 0,  94,  99}, {0x2e17, 0,  86,  93},
    /*  95 */ {0x56a8, 1,  96,  95}, {0x4f46, 0,  97, 101}, {0x47e5, 0,  98, 102}, {0x41cf, 0,  99, 103}, {0x3c3d, 0, 100, 104},
    /* 100 */ {0x375e, 0,  93,  99}, {0x5231, 0, 102, 105}, {0x4c0f, 0, 103, 106}, {0x4639, 0, 104, 107}, {0x415e, 0,  99, 103},
    /* 105 */ {0x5627, 1, 106, 105}, {0x50e7, 0, 107, 108}, {0x4b85, 0, 103, 109}, {0x5597, 0, 109, 110}, {0x504f, 0, 107, 111},
    /* 110 */ {0x5a10, 1, 111, 110}, {0x5522, 0, 109, 112}, {0x59eb, 1, 111, 112}
  };
  const struct JPEG_arithmetic_decoder_state * state = states + (index ? absolute_value(*index) : 0);
  unsigned decoded, predicted = index && (*index < 0); // predict the MPS; decode a 1 if the prediction is false
  *accumulator -= state -> probability;
  if (*accumulator > (*current >> 8)) {
    if (*accumulator >= 0x8000u) return predicted;
    decoded = *accumulator < state -> probability;
  } else {
    decoded = *accumulator >= state -> probability;
    *current -= (uint32_t) *accumulator << 8;
    *accumulator = state -> probability;
  }
  if (index)
    if (decoded) {
      *index = predicted ? -state -> next_LPS : state -> next_LPS;
      if (state -> switch_MPS) *index = -*index;
    } else
      *index = predicted ? -state -> next_MPS : state -> next_MPS;
  // normalize the counters, consuming new data if needed
  do {
    if (!*bits) {
      unsigned char data = 0;
      if (*remaining) {
        data = context -> data[(*offset) ++];
        -- *remaining;
      }
      if (data == 0xff) while (*remaining) {
        -- *remaining;
        if (context -> data[(*offset) ++] != 0xff) break;
      }
      *current |= data;
      *bits = 8;
    }
    *accumulator <<= 1;
    *current = (*current << 1) & 0xffffffu;
    -- *bits;
  } while (*accumulator < 0x8000u);
  return predicted ^ decoded;
}
