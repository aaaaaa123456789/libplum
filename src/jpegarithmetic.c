#include "proto.h"

void decompress_JPEG_arithmetic_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                      size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                      unsigned char last, int differential) {
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t accumulator = 0;
    uint32_t current = 0;
    unsigned char bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexesDC[4][49] = {0};
    signed char indexesAC[4][245] = {0};
    uint16_t prevDC[4] = {0};
    uint16_t prevdiff[4] = {0};
    while (units --) {
      int16_t (* outputunit)[64];
      for (const unsigned char * decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputunit = state -> current_block[decodepos[1]];
          break;
        case MCU_NEXT_ROW:
          outputunit += state -> row_offset[decodepos[1]];
          break;
        default: {
          bool prevzero = false; // was the previous coefficient zero?
          for (uint_fast8_t p = first; p <= last; p ++) {
            if (skipunits)
              p[*outputunit] = 0;
            else if (p) {
              unsigned char conditioning = tables -> arithmetic[components[*decodepos].tableAC + 4];
              signed char * index = indexesAC[components[*decodepos].tableAC] + 3 * (p - 1);
              if (!prevzero && next_JPEG_arithmetic_bit(context, &offset, &remaining, index, &current, &accumulator, &bits)) {
                p[*outputunit] = 0;
                skipunits ++;
              } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 1, &current, &accumulator, &bits)) {
                p[*outputunit] = next_JPEG_arithmetic_value(context, &offset, &remaining, &current, &accumulator, &bits, indexesAC[components[*decodepos].tableAC],
                                                            1, p, conditioning);
                prevzero = false;
              } else {
                p[*outputunit] = 0;
                prevzero = true;
              }
            } else {
              unsigned char conditioning = tables -> arithmetic[components[*decodepos].tableDC];
              unsigned char category = classify_JPEG_arithmetic_value(prevdiff[*decodepos], conditioning);
              if (next_JPEG_arithmetic_bit(context, &offset, &remaining, indexesDC[components[*decodepos].tableDC] + 4 * category, &current, &accumulator, &bits))
                prevdiff[*decodepos] = next_JPEG_arithmetic_value(context, &offset, &remaining, &current, &accumulator, &bits,
                                                                  indexesDC[components[*decodepos].tableDC], 0, category, conditioning);
              else
                prevdiff[*decodepos] = 0;
              if (differential)
                **outputunit = make_signed_16(prevdiff[*decodepos]);
              else
                prevDC[*decodepos] = **outputunit = make_signed_16(prevDC[*decodepos] + prevdiff[*decodepos]);
            }
            p[*outputunit] = make_signed_16((uint16_t) p[*outputunit] << shift);
          }
          outputunit ++;
          if (skipunits) skipunits --;
        }
      }
      if (++ colcount == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (uint_fast8_t p = 0; p < 4; p ++) if (state -> current_block[p]) {
        state -> current_block[p] += state -> unit_offset[p];
        if (!colcount) state -> current_block[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_arithmetic_bit_scan (struct context * context, struct JPEG_decompressor_state * restrict state, size_t rowunits,
                                          const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                          unsigned char last) {
  // this function is very similar to decompress_JPEG_arithmetic_scan, but it only decodes the next bit for already-initialized data
  if (last && !first) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t accumulator = 0;
    uint32_t current = 0;
    unsigned char bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexes[4][189] = {0}; // most likely very few will be actually used, but allocate for the worst case
    while (units --) {
      int16_t (* outputunit)[64];
      for (const unsigned char * decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
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
            unsigned char lastnonzero; // last non-zero coefficient up to the previous scan (for the same component)
            for (lastnonzero = 63; lastnonzero; lastnonzero --) if (lastnonzero[*outputunit]) break;
            bool prevzero = false; // was the previous coefficient zero?
            for (uint_fast8_t p = first; p <= last; p ++) {
              signed char * index = indexes[components[*decodepos].tableAC] + 3 * (p - 1);
              if (!prevzero && p > lastnonzero && next_JPEG_arithmetic_bit(context, &offset, &remaining, index, &current, &accumulator, &bits)) break;
              if (p[*outputunit]) {
                prevzero = false;
                if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 2, &current, &accumulator, &bits))
                  if (p[*outputunit] < 0)
                    p[*outputunit] -= 1 << shift;
                  else
                    p[*outputunit] += 1 << shift;
              } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, index + 1, &current, &accumulator, &bits)) {
                prevzero = false;
                p[*outputunit] = next_JPEG_arithmetic_bit(context, &offset, &remaining, NULL, &current, &accumulator, &bits) ?
                                 make_signed_16(0xffffu << shift) : (1 << shift);
              } else
                prevzero = true;
            }
          } else if (next_JPEG_arithmetic_bit(context, &offset, &remaining, NULL, &current, &accumulator, &bits))
            **outputunit += 1 << shift;
          outputunit ++;
      }
      if (++ colcount == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (uint_fast8_t p = 0; p < 4; p ++) if (state -> current_block[p]) {
        state -> current_block[p] += state -> unit_offset[p];
        if (!colcount) state -> current_block[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_arithmetic_lossless_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                               size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned char predictor,
                                               unsigned precision) {
  bool scancomponents[4] = {0};
  for (uint_fast8_t p = 0; state -> MCU[p] != MCU_END_LIST; p ++) if (state -> MCU[p] < 4) scancomponents[state -> MCU[p]] = true;
  uint16_t * rowdifferences[4] = {0};
  for (uint_fast8_t p = 0; p < 4; p ++) if (scancomponents[p])
    rowdifferences[p] = ctxmalloc(context, sizeof **rowdifferences * rowunits * ((state -> component_count > 1) ? components[p].scaleH : 1));
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t offset = *(offsets ++);
    size_t remaining = *(offsets ++);
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint16_t accumulator = 0;
    uint32_t current = 0;
    unsigned char bits = 0;
    initialize_JPEG_arithmetic_counters(context, &offset, &remaining, &current);
    signed char indexes[4][158] = {0};
    for (uint_fast8_t p = 0; p < 4; p ++) if (scancomponents[p])
      for (uint_fast16_t x = 0; x < (rowunits * ((state -> component_count > 1) ? components[p].scaleH : 1)); x ++) rowdifferences[p][x] = 0;
    uint16_t coldifferences[4][4] = {0};
    while (units --) {
      uint_fast16_t x, y;
      uint16_t * outputpos;
      for (const unsigned char * decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputpos = state -> current_value[decodepos[1]];
          x = colcount * ((state -> component_count > 1) ? components[decodepos[1]].scaleH : 1);
          y = 0;
          break;
        case MCU_NEXT_ROW:
          outputpos += state -> row_offset[decodepos[1]];
          x = colcount * ((state -> component_count > 1) ? components[decodepos[1]].scaleH : 1);
          y ++;
          break;
        default:
          if (skipunits) {
            *(outputpos ++) = 0;
            skipunits --;
          } else {
            unsigned char conditioning = tables -> arithmetic[components[*decodepos].tableDC];
            size_t rowsize = rowunits * ((state -> component_count > 1) ? components[*decodepos].scaleH : 1);
            uint16_t difference, predicted = predict_JPEG_lossless_sample(outputpos, rowsize, !x, !(y || rowcount), predictor, precision);
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
      if (++ colcount == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
        memset(coldifferences, 0, sizeof coldifferences);
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (uint_fast8_t p = 0; p < 4; p ++) if (state -> current_value[p]) {
        state -> current_value[p] += state -> unit_offset[p];
        if (!colcount) state -> current_value[p] += state -> unit_row_offset[p];
      }
    }
    if (remaining || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  for (uint_fast8_t p = 0; p < state -> component_count; p ++) ctxfree(context, rowdifferences[p]);
}

void initialize_JPEG_arithmetic_counters (struct context * context, size_t * restrict offset, size_t * restrict remaining, uint32_t * restrict current) {
  for (uint_fast8_t loopcount = 0; loopcount < 2; loopcount ++) {
    unsigned char data = 0;
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
  bool negative = next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits);
  index = (mode == 1) ? indexes + 3 * reference - 1 : (index + 1 + negative);
  uint_fast8_t size = next_JPEG_arithmetic_bit(context, offset, remaining, index, current, accumulator, bits);
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

bool next_JPEG_arithmetic_bit (struct context * context, size_t * restrict offset, size_t * restrict remaining, signed char * restrict index,
                               uint32_t * restrict current, uint16_t * restrict accumulator, unsigned char * restrict bits) {
  // negative state index: MPS = 1; null state: use 0 and don't update
  // index 0 implies MPS = 0; there's no way to encode index = 0 and MPS = 1 (because that'd be state = -0), but that state cannot happen
  static const struct JPEG_arithmetic_decoder_state states[] = {
    /*   0 */ {0x5a1d,  true,   1,   1}, {0x2586, false,   2,  14}, {0x1114, false,   3,  16}, {0x080b, false,   4,  18}, {0x03d8, false,   5,  20},
    /*   5 */ {0x01da, false,   6,  23}, {0x00e5, false,   7,  25}, {0x006f, false,   8,  28}, {0x0036, false,   9,  30}, {0x001a, false,  10,  33},
    /*  10 */ {0x000d, false,  11,  35}, {0x0006, false,  12,   9}, {0x0003, false,  13,  10}, {0x0001, false,  13,  12}, {0x5a7f,  true,  15,  15},
    /*  15 */ {0x3f25, false,  16,  36}, {0x2cf2, false,  17,  38}, {0x207c, false,  18,  39}, {0x17b9, false,  19,  40}, {0x1182, false,  20,  42},
    /*  20 */ {0x0cef, false,  21,  43}, {0x09a1, false,  22,  45}, {0x072f, false,  23,  46}, {0x055c, false,  24,  48}, {0x0406, false,  25,  49},
    /*  25 */ {0x0303, false,  26,  51}, {0x0240, false,  27,  52}, {0x01b1, false,  28,  54}, {0x0144, false,  29,  56}, {0x00f5, false,  30,  57},
    /*  30 */ {0x00b7, false,  31,  59}, {0x008a, false,  32,  60}, {0x0068, false,  33,  62}, {0x004e, false,  34,  63}, {0x003b, false,  35,  32},
    /*  35 */ {0x002c, false,   9,  33}, {0x5ae1,  true,  37,  37}, {0x484c, false,  38,  64}, {0x3a0d, false,  39,  65}, {0x2ef1, false,  40,  67},
    /*  40 */ {0x261f, false,  41,  68}, {0x1f33, false,  42,  69}, {0x19a8, false,  43,  70}, {0x1518, false,  44,  72}, {0x1177, false,  45,  73},
    /*  45 */ {0x0e74, false,  46,  74}, {0x0bfb, false,  47,  75}, {0x09f8, false,  48,  77}, {0x0861, false,  49,  78}, {0x0706, false,  50,  79},
    /*  50 */ {0x05cd, false,  51,  48}, {0x04de, false,  52,  50}, {0x040f, false,  53,  50}, {0x0363, false,  54,  51}, {0x02d4, false,  55,  52},
    /*  55 */ {0x025c, false,  56,  53}, {0x01f8, false,  57,  54}, {0x01a4, false,  58,  55}, {0x0160, false,  59,  56}, {0x0125, false,  60,  57},
    /*  60 */ {0x00f6, false,  61,  58}, {0x00cb, false,  62,  59}, {0x00ab, false,  63,  61}, {0x008f, false,  32,  61}, {0x5b12,  true,  65,  65},
    /*  65 */ {0x4d04, false,  66,  80}, {0x412c, false,  67,  81}, {0x37d8, false,  68,  82}, {0x2fe8, false,  69,  83}, {0x293c, false,  70,  84},
    /*  70 */ {0x2379, false,  71,  86}, {0x1edf, false,  72,  87}, {0x1aa9, false,  73,  87}, {0x174e, false,  74,  72}, {0x1424, false,  75,  72},
    /*  75 */ {0x119c, false,  76,  74}, {0x0f6b, false,  77,  74}, {0x0d51, false,  78,  75}, {0x0bb6, false,  79,  77}, {0x0a40, false,  48,  77},
    /*  80 */ {0x5832,  true,  81,  80}, {0x4d1c, false,  82,  88}, {0x438e, false,  83,  89}, {0x3bdd, false,  84,  90}, {0x34ee, false,  85,  91},
    /*  85 */ {0x2eae, false,  86,  92}, {0x299a, false,  87,  93}, {0x2516, false,  71,  86}, {0x5570,  true,  89,  88}, {0x4ca9, false,  90,  95},
    /*  90 */ {0x44d9, false,  91,  96}, {0x3e22, false,  92,  97}, {0x3824, false,  93,  99}, {0x32b4, false,  94,  99}, {0x2e17, false,  86,  93},
    /*  95 */ {0x56a8,  true,  96,  95}, {0x4f46, false,  97, 101}, {0x47e5, false,  98, 102}, {0x41cf, false,  99, 103}, {0x3c3d, false, 100, 104},
    /* 100 */ {0x375e, false,  93,  99}, {0x5231, false, 102, 105}, {0x4c0f, false, 103, 106}, {0x4639, false, 104, 107}, {0x415e, false,  99, 103},
    /* 105 */ {0x5627,  true, 106, 105}, {0x50e7, false, 107, 108}, {0x4b85, false, 103, 109}, {0x5597, false, 109, 110}, {0x504f, false, 107, 111},
    /* 110 */ {0x5a10,  true, 111, 110}, {0x5522, false, 109, 112}, {0x59eb,  true, 111, 112}
  };
  const struct JPEG_arithmetic_decoder_state * state = states + (index ? absolute_value(*index) : 0);
  bool decoded, predicted = index && *index < 0; // predict the MPS; decode a 1 if the prediction is false
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
    if (decoded)
      *index = (predicted != state -> switch_MPS) ? -state -> next_LPS : state -> next_LPS;
    else
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
  return predicted != decoded;
}
