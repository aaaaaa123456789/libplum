#include "proto.h"

void load_PNG_frame (struct context * context, const size_t * chunks, uint32_t frame, const uint64_t * palette, uint8_t max_palette_index,
                     uint8_t imagetype, uint8_t bitdepth, int interlaced, uint64_t background, uint64_t transparent) {
  void * data = load_PNG_frame_part(context, chunks, palette ? max_palette_index : -1, imagetype, bitdepth, interlaced,
                                    context -> image -> width, context -> image -> height, frame ? 4 : 0);
  if (palette)
    write_palette_framebuffer_to_image(context, data, palette, frame, context -> image -> color_format, 0xff); // 0xff to avoid a redundant range check
  else {
    if (transparent != 0xffffffffffffffffu) {
      uint64_t * current = data;
      size_t count = (size_t) context -> image -> width * context -> image -> height;
      for (; count; count --, current ++) if (*current == transparent) *current = background | 0xffff000000000000u;
    }
    write_framebuffer_to_image(context -> image, data, frame, context -> image -> color_format);
  }
  ctxfree(context, data);
}

void * load_PNG_frame_part (struct context * context, const size_t * chunks, int max_palette_index, uint8_t imagetype, uint8_t bitdepth, int interlaced,
                            uint32_t width, uint32_t height, size_t chunkoffset) {
  // max_palette_index < 0: no palette (return uint64_t *); otherwise, use a palette (return uint8_t *)
  size_t p = 0, total_compressed_size = 0;
  const size_t * chunk;
  for (chunk = chunks; *chunk; chunk ++) total_compressed_size += read_be32_unaligned(context -> data + *chunk - 8) - chunkoffset;
  char * compressed = ctxmalloc(context, total_compressed_size);
  for (chunk = chunks; *chunk; chunk ++) {
    size_t current = read_be32_unaligned(context -> data + *chunk - 8) - chunkoffset;
    memcpy(compressed + p, context -> data + *chunk + chunkoffset, current);
    p += current;
  }
  void * result;
  if (max_palette_index < 0)
    result = load_PNG_raw_frame(context, compressed, total_compressed_size, width, height, imagetype, bitdepth, interlaced);
  else
    result = load_PNG_palette_frame(context, compressed, total_compressed_size, width, height, bitdepth, max_palette_index, interlaced);
  ctxfree(context, compressed);
  return result;
}

uint8_t * load_PNG_palette_frame (struct context * context, const void * compressed, size_t compressed_size, uint32_t width, uint32_t height, uint8_t bitdepth,
                                  uint8_t max_palette_index, int interlaced) {
  // imagetype must be 3 here
  uint8_t * result = ctxmalloc(context, (size_t) width * height);
  unsigned char * decompressed;
  if (interlaced) {
    size_t widths[] = {(width + 7) / 8, (width + 3) / 8, (width + 3) / 4, (width + 1) / 4, (width + 1) / 2, width / 2, width};
    size_t heights[] = {(height + 7) / 8, (height + 7) / 8, (height + 3) / 8, (height + 3) / 4, (height + 1) / 4, (height + 1) / 2, height / 2};
    const unsigned char coordsH[] = {0, 4, 0, 2, 0, 1, 0};
    const unsigned char coordsV[] = {0, 0, 4, 0, 2, 0, 1};
    const unsigned char offsetsH[] = {8, 8, 4, 4, 2, 2, 1};
    const unsigned char offsetsV[] = {8, 8, 8, 4, 4, 2, 2};
    size_t rowsize, cumulative_size = 0;
    uint_fast8_t pass;
    for (pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      rowsize = ((size_t) widths[pass] * bitdepth + 15) / 8;
      cumulative_size += heights[pass] * rowsize;
    }
    decompressed = decompress_PNG_data(context, compressed, compressed_size, cumulative_size);
    unsigned char * current = decompressed;
    uint_fast32_t row, col;
    unsigned char * rowdata = ctxmalloc(context, width);
    for (pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      rowsize = ((size_t) widths[pass] * bitdepth + 15) / 8;
      remove_PNG_filter(context, current, widths[pass], heights[pass], 3, bitdepth);
      for (row = 0; row < heights[pass]; row ++) {
        expand_bitpacked_PNG_data(rowdata, current + 1, widths[pass], bitdepth);
        current += rowsize;
        for (col = 0; col < widths[pass]; col ++) result[(row * offsetsV[pass] + coordsV[pass]) * width + col * offsetsH[pass] + coordsH[pass]] = rowdata[col];
      }
    }
    ctxfree(context, rowdata);
  } else {
    size_t row, rowsize = ((size_t) width * bitdepth + 15) / 8;
    decompressed = decompress_PNG_data(context, compressed, compressed_size, rowsize * height);
    remove_PNG_filter(context, decompressed, width, height, 3, bitdepth);
    for (row = 0; row < height; row ++) expand_bitpacked_PNG_data(result + row * width, decompressed + row * rowsize + 1, width, bitdepth);
  }
  ctxfree(context, decompressed);
  size_t p;
  for (p = 0; p < ((size_t) width * height); p ++) if (result[p] > max_palette_index) throw(context, PLUM_ERR_INVALID_COLOR_INDEX);
  return result;
}

uint64_t * load_PNG_raw_frame (struct context * context, const void * compressed, size_t compressed_size, uint32_t width, uint32_t height, uint8_t imagetype,
                               uint8_t bitdepth, int interlaced) {
  // imagetype is not 3 here
  uint64_t * result = ctxmalloc(context, sizeof *result * width * height);
  unsigned char * decompressed;
  size_t pixelsize = bitdepth / 8; // 0 will be treated as a special value
  pixelsize *= (imagetype >> 1)[(unsigned char []) {1, 3, 2, 4}];
  if (interlaced) {
    size_t widths[] = {(width + 7) / 8, (width + 3) / 8, (width + 3) / 4, (width + 1) / 4, (width + 1) / 2, width / 2, width};
    size_t heights[] = {(height + 7) / 8, (height + 7) / 8, (height + 3) / 8, (height + 3) / 4, (height + 1) / 4, (height + 1) / 2, height / 2};
    const unsigned char coordsH[] = {0, 4, 0, 2, 0, 1, 0};
    const unsigned char coordsV[] = {0, 0, 4, 0, 2, 0, 1};
    const unsigned char offsetsH[] = {8, 8, 4, 4, 2, 2, 1};
    const unsigned char offsetsV[] = {8, 8, 8, 4, 4, 2, 2};
    size_t rowsize, cumulative_size = 0;
    uint_fast8_t pass;
    for (pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      rowsize = pixelsize ? pixelsize * widths[pass] + 1 : (((size_t) widths[pass] * bitdepth + 15) / 8);
      cumulative_size += rowsize * heights[pass];
    }
    decompressed = decompress_PNG_data(context, compressed, compressed_size, cumulative_size);
    unsigned char * current = decompressed;
    for (pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      load_PNG_raw_frame_pass(context, current, result, heights[pass], widths[pass], width, imagetype, bitdepth, coordsH[pass], coordsV[pass],
                              offsetsH[pass], offsetsV[pass]);
      rowsize = pixelsize ? pixelsize * widths[pass] + 1 : (((size_t) widths[pass] * bitdepth + 15) / 8);
      current += rowsize * heights[pass];
    }
  } else {
    size_t rowsize = pixelsize ? pixelsize * width + 1 : (((size_t) width * bitdepth + 15) / 8);
    decompressed = decompress_PNG_data(context, compressed, compressed_size, rowsize * height);
    load_PNG_raw_frame_pass(context, decompressed, result, height, width, width, imagetype, bitdepth, 0, 0, 1, 1);
  }
  ctxfree(context, decompressed);
  return result;
}

void load_PNG_raw_frame_pass (struct context * context, unsigned char * restrict data, uint64_t * restrict output, uint32_t height, uint32_t width,
                              uint32_t fullwidth, uint8_t imagetype, uint8_t bitdepth, unsigned char coordH, unsigned char coordV, unsigned char offsetH,
                              unsigned char offsetV) {
  size_t pixelsize = bitdepth / 8; // 0 will be treated as a special value
  pixelsize *= (imagetype >> 1)[(unsigned char []) {1, 3, 2, 4}];
  size_t rowsize = pixelsize ? pixelsize * width + 1 : (((size_t) width * bitdepth + 15) / 8);
  uint_fast32_t row, col;
  remove_PNG_filter(context, data, width, height, imagetype, bitdepth);
  unsigned char * rowdata;
  uint64_t * rowoutput;
  for (row = 0; row < height; row ++) {
    rowoutput = output + (row * offsetV + coordV) * fullwidth;
    rowdata = data + 1;
    switch (bitdepth + imagetype) {
      // since bitdepth must be 8 or 16 here unless imagetype is 0, all combinations are unique
      case 8:
        for (col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) *(rowdata ++) * 0x10101010101u;
        break;
      case 10:
        for (col = 0; col < width; col ++) {
          uint64_t color = *(rowdata ++);
          color |= (uint64_t) *(rowdata ++) << 16;
          color |= (uint64_t) *(rowdata ++) << 32;
          rowoutput[col * offsetH + coordH] = color * 0x101;
        }
        break;
      case 12:
        for (col = 0; col < width; col ++) {
          uint64_t color = (uint64_t) *(rowdata ++) * 0x10101010101u;
          color |= (uint64_t) (*(rowdata ++) ^ 0xff) * 0x101000000000000u;
          rowoutput[col * offsetH + coordH] = color;
        }
        break;
      case 14:
        for (col = 0; col < width; col ++) {
          uint64_t color = *(rowdata ++);
          color |= (uint64_t) *(rowdata ++) << 16;
          color |= (uint64_t) *(rowdata ++) << 32;
          color |= (uint64_t) (*(rowdata ++) ^ 0xff) << 48;
          rowoutput[col * offsetH + coordH] = color * 0x101;
        }
        break;
      case 16:
        for (col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) read_be16_unaligned(rowdata + 2 * col) * 0x100010001u;
        break;
      case 18:
        for (col = 0; col < width; col ++) {
          uint64_t color = (uint64_t) *(rowdata ++) << 8;
          color |= *(rowdata ++);
          color |= (uint64_t) *(rowdata ++) << 24;
          color |= (uint64_t) *(rowdata ++) << 16;
          color |= (uint64_t) *(rowdata ++) << 40;
          color |= (uint64_t) *(rowdata ++) << 32;
          rowoutput[col * offsetH + coordH] = color;
        }
        break;
      case 20:
        for (col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = (uint64_t) read_be16_unaligned(rowdata + 4 * col) * 0x100010001u +
                                              ((uint64_t) ~read_be16_unaligned(rowdata + 4 * col + 2) << 48);
        break;
      case 22:
        for (col = 0; col < width; col ++) {
          uint64_t color = (uint64_t) *(rowdata ++) << 8;
          color |= *(rowdata ++);
          color |= (uint64_t) *(rowdata ++) << 24;
          color |= (uint64_t) *(rowdata ++) << 16;
          color |= (uint64_t) *(rowdata ++) << 40;
          color |= (uint64_t) *(rowdata ++) << 32;
          color |= (uint64_t) (*(rowdata ++) ^ 0xff) << 56;
          color |= (uint64_t) (*(rowdata ++) ^ 0xff) << 48;
          rowoutput[col * offsetH + coordH] = color;
        }
        break;
      default: {
        unsigned char * buffer = ctxmalloc(context, width);
        expand_bitpacked_PNG_data(buffer, rowdata, width, bitdepth);
        for (col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) bitextend16(buffer[col], bitdepth) * 0x100010001u;
        ctxfree(context, buffer);
      }
    }
    data += rowsize;
  }
}

void expand_bitpacked_PNG_data (unsigned char * restrict result, const unsigned char * restrict source, size_t count, uint8_t bitdepth) {
  unsigned char remainder;
  switch (bitdepth) {
    case 1:
      for (; count > 7; count -= 8) {
        *(result ++) = !!(*source & 0x80);
        *(result ++) = !!(*source & 0x40);
        *(result ++) = !!(*source & 0x20);
        *(result ++) = !!(*source & 0x10);
        *(result ++) = !!(*source & 8);
        *(result ++) = !!(*source & 4);
        *(result ++) = !!(*source & 2);
        *(result ++) = *(source ++) & 1;
      }
      if (count) for (remainder = *source; count; count --, remainder <<= 1) *(result ++) = remainder >> 7;
      break;
    case 2:
      for (; count > 3; count -= 4) {
        *(result ++) = *source >> 6;
        *(result ++) = (*source >> 4) & 3;
        *(result ++) = (*source >> 2) & 3;
        *(result ++) = *(source ++) & 3;
      }
      if (count) for (remainder = *source; count; count --, remainder <<= 2) *(result ++) = remainder >> 6;
      break;
    case 4:
      for (; count > 1; count -= 2) {
        *(result ++) = *source >> 4;
        *(result ++) = *(source ++) & 15;
      }
      if (count) *result = *source >> 4;
      break;
    default:
      memcpy(result, source, count);
  }
}

void remove_PNG_filter (struct context * context, unsigned char * restrict data, uint32_t width, uint32_t height, uint8_t imagetype, uint8_t bitdepth) {
  ptrdiff_t pixelsize = bitdepth / 8;
  if (imagetype != 3) pixelsize *= (imagetype >> 1)[(unsigned char []) {1, 3, 2, 4}];
  if (!pixelsize) {
    pixelsize = 1;
    width = ((size_t) width * bitdepth + 7) / 8;
  }
  ptrdiff_t p, rowsize = pixelsize * width + 1;
  uint_fast32_t row;
  for (row = 0; row < height; row ++) {
    unsigned char * rowdata = data + 1;
    switch (*data) {
      case 4:
        for (p = 0; p < (pixelsize * width); p ++) {
          int top = row ? rowdata[p - rowsize] : 0, left = (p < pixelsize) ? 0 : rowdata[p - pixelsize];
          int diagonal = (row && (p >= pixelsize)) ? rowdata[p - pixelsize - rowsize] : 0;
          int topdiff = absolute_value(left - diagonal), leftdiff = absolute_value(top - diagonal), diagdiff = absolute_value(left + top - diagonal * 2);
          rowdata[p] += ((leftdiff <= topdiff) && (leftdiff <= diagdiff)) ? left : (topdiff <= diagdiff) ? top : diagonal;
        }
        break;
      case 3:
        if (row) {
          for (p = 0; p < pixelsize; p ++) rowdata[p] += rowdata[p - rowsize] >> 1;
          for (; p < (pixelsize * width); p ++) rowdata[p] += (rowdata[p - pixelsize] + rowdata[p - rowsize]) >> 1;
        } else
          for (p = pixelsize; p < (pixelsize * width); p ++) rowdata[p] += rowdata[p - pixelsize] >> 1;
        break;
      case 2:
        if (row) for (p = 0; p < (pixelsize * width); p ++) rowdata[p] += rowdata[p - rowsize];
        break;
      case 1:
        for (p = pixelsize; p < (pixelsize * width); p ++) rowdata[p] += rowdata[p - pixelsize];
      case 0:
        break;
      default:
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    data += rowsize;
  }
}
