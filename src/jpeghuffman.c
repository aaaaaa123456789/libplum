#include "proto.h"

void decompress_JPEG_Huffman_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                   size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                   unsigned char last, int differential) {
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t colcount = 0, rowcount = 0, skipcount = 0, skipunits = 0;
    const unsigned char * data = context -> data + *(offsets ++);
    size_t count = *(offsets ++);
    uint16_t prevDC[4] = {0};
    int16_t nextvalue = 0;
    uint32_t dataword = 0;
    uint8_t bits = 0;
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
          for (uint_fast8_t p = first; p <= last; p ++) {
            if (!(skipcount || nextvalue || skipunits)) {
              unsigned char decompressed;
              if (p) {
                decompressed = next_JPEG_Huffman_value(context, &data, &count, &dataword, &bits, tables -> Huffman[components[*decodepos].tableAC + 4]);
                if (decompressed & 15)
                  skipcount = decompressed >> 4;
                else if (decompressed == 0xf0)
                  skipcount = 16;
                else
                  skipunits = (1u << (decompressed >> 4)) + shift_in_right_JPEG(context, decompressed >> 4, &dataword, &bits, &data, &count);
                decompressed &= 15;
              } else {
                decompressed = next_JPEG_Huffman_value(context, &data, &count, &dataword, &bits, tables -> Huffman[components[*decodepos].tableDC]);
                if (decompressed > 15) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
              }
              if (decompressed) {
                uint_fast16_t extrabits = shift_in_right_JPEG(context, decompressed, &dataword, &bits, &data, &count);
                if (!(extrabits >> (decompressed - 1))) nextvalue = make_signed_16(1u - (1u << decompressed));
                nextvalue = make_signed_16(nextvalue + extrabits);
              }
            }
            if (skipcount || skipunits) {
              p[*outputunit] = 0;
              if (skipcount) skipcount --;
            } else {
              p[*outputunit] = nextvalue * (1 << shift);
              nextvalue = 0;
            }
            if (!(p || differential)) prevDC[*decodepos] = **outputunit = make_signed_16(prevDC[*decodepos] + (uint16_t) **outputunit);
          }
          outputunit ++;
          if (skipunits) skipunits --;
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
    if (count || skipcount || skipunits || nextvalue) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_Huffman_bit_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                       size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned shift, unsigned char first,
                                       unsigned char last) {
  // this function is essentially the same as decompress_JPEG_Huffman_scan, but it uses already-initialized component data, and it decodes one bit at a time
  if (last && !first) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    size_t colcount = 0, rowcount = 0, skipcount = 0, skipunits = 0;
    const unsigned char * data = context -> data + *(offsets ++);
    size_t count = *(offsets ++);
    int16_t nextvalue = 0;
    uint32_t dataword = 0;
    uint8_t bits = 0;
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
          if (first) {
            for (uint_fast8_t p = first; p <= last; p ++) {
              if (!(skipcount || nextvalue || skipunits)) {
                unsigned char decompressed = next_JPEG_Huffman_value(context, &data, &count, &dataword, &bits,
                                                                     tables -> Huffman[components[*decodepos].tableAC + 4]);
                if (decompressed & 15)
                  skipcount = decompressed >> 4;
                else if (decompressed == 0xf0)
                  skipcount = 16;
                else
                  skipunits = (1u << (decompressed >> 4)) + shift_in_right_JPEG(context, decompressed >> 4, &dataword, &bits, &data, &count);
                decompressed &= 15;
                if (decompressed) {
                  uint_fast16_t extrabits = shift_in_right_JPEG(context, decompressed, &dataword, &bits, &data, &count);
                  if (!(extrabits >> (decompressed - 1))) nextvalue = make_signed_16(1u - (1u << decompressed));
                  nextvalue = make_signed_16(nextvalue + extrabits);
                }
              }
              if (p[*outputunit]) {
                if (shift_in_right_JPEG(context, 1, &dataword, &bits, &data, &count))
                  if (p[*outputunit] < 0)
                    p[*outputunit] -= 1 << shift;
                  else
                    p[*outputunit] += 1 << shift;
              } else if (skipcount || skipunits) {
                if (skipcount) skipcount --;
              } else {
                p[*outputunit] = nextvalue * (1 << shift);
                nextvalue = 0;
              }
            }
          } else if (!skipunits)
            **outputunit += shift_in_right_JPEG(context, 1, &dataword, &bits, &data, &count) << shift;
          outputunit ++;
          if (skipunits) skipunits --;
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
    if (count || skipcount || skipunits || nextvalue) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void decompress_JPEG_Huffman_lossless_scan (struct context * context, struct JPEG_decompressor_state * restrict state, const struct JPEG_decoder_tables * tables,
                                            size_t rowunits, const struct JPEG_component_info * components, const size_t * offsets, unsigned char predictor,
                                            unsigned precision) {
  for (size_t restart_interval = 0; restart_interval <= state -> restart_count; restart_interval ++) {
    size_t units = (restart_interval == state -> restart_count) ? state -> last_size : state -> restart_size;
    if (!units) break;
    const unsigned char * data = context -> data + *(offsets ++);
    size_t count = *(offsets ++);
    size_t colcount = 0, rowcount = 0, skipunits = 0;
    uint32_t dataword = 0;
    uint8_t bits = 0;
    while (units --) {
      uint16_t * outputpos;
      int leftmost, topmost;
      for (const unsigned char * decodepos = state -> MCU; *decodepos != MCU_END_LIST; decodepos ++) switch (*decodepos) {
        case MCU_ZERO_COORD:
          outputpos = state -> current_value[decodepos[1]];
          leftmost = topmost = 1;
          break;
        case MCU_NEXT_ROW:
          outputpos += state -> row_offset[decodepos[1]];
          leftmost = 1;
          topmost = 0;
          break;
        default:
          if (skipunits) {
            *(outputpos ++) = 0;
            skipunits --;
          } else {
            size_t rowsize = rowunits * ((state -> component_count > 1) ? components[*decodepos].scaleH : 1);
            uint16_t difference, predicted = predict_JPEG_lossless_sample(outputpos, rowsize, leftmost && !colcount, topmost && !rowcount, predictor, precision);
            unsigned char diffsize = next_JPEG_Huffman_value(context, &data, &count, &dataword, &bits, tables -> Huffman[components[*decodepos].tableDC]);
            if (diffsize > 16) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
            switch (diffsize) {
              case 0:
                difference = 0;
                break;
              case 16:
                difference = 0x8000u;
                break;
              default:
                difference = shift_in_right_JPEG(context, diffsize, &dataword, &bits, &data, &count);
                if (!(difference >> (diffsize - 1))) difference -= (1u << diffsize) - 1;
            }
            *(outputpos ++) = predicted + difference;
          }
          leftmost = 0;
      }
      if (++ colcount == rowunits) {
        colcount = 0;
        rowcount ++;
        if (rowcount == state -> row_skip_index) skipunits += (rowunits - state -> column_skip_count) * state -> row_skip_count;
      }
      if (colcount == state -> column_skip_index) skipunits += state -> column_skip_count;
      for (uint_fast8_t p = 0; p < 4; p ++) if (state -> current_value[p]) {
        state -> current_value[p] += state -> unit_offset[p];
        if (!colcount) state -> current_value[p] += state -> unit_row_offset[p];
      }
    }
    if (count || skipunits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

unsigned char next_JPEG_Huffman_value (struct context * context, const unsigned char ** data, size_t * restrict count, uint32_t * restrict dataword,
                                       uint8_t * restrict bits, const short * table) {
  unsigned short index = 0;
  while (1) {
    index += shift_in_right_JPEG(context, 1, dataword, bits, data, count);
    if (table[index] >= 0) return table[index];
    if (table[index] == -1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    index = -table[index];
  }
}
