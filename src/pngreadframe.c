#include "proto.h"

void load_PNG_frame (struct context * context, const size_t * chunks, uint32_t frame, const uint64_t * palette, uint8_t max_palette_index,
                     uint8_t imagetype, uint8_t bitdepth, bool interlaced, uint64_t background, uint64_t transparent) {
  void * data = load_PNG_frame_part(context, chunks, palette ? max_palette_index : -1, imagetype, bitdepth, interlaced,
                                    context -> image -> width, context -> image -> height, frame ? 4 : 0);
  if (palette)
    write_palette_framebuffer_to_image(context, data, palette, frame, context -> image -> color_format, 0xff); // 0xff to avoid a redundant range check
  else {
    if (transparent != 0xffffffffffffffffu) {
      size_t count = (size_t) context -> image -> width * context -> image -> height;
      for (uint64_t * current = data; count; count --, current ++) if (*current == transparent) *current = background | 0xffff000000000000u;
    }
    write_framebuffer_to_image(context -> image, data, frame, context -> image -> color_format);
  }
  ctxfree(context, data);
}

void * load_PNG_frame_part (struct context * context, const size_t * chunks, int max_palette_index, uint8_t imagetype, uint8_t bitdepth, bool interlaced,
                            uint32_t width, uint32_t height, size_t chunkoffset) {
  // max_palette_index < 0: no palette (return uint64_t *); otherwise, use a palette (return uint8_t *)
  size_t p = 0, total_compressed_size = 0;
  for (const size_t * chunk = chunks; *chunk; chunk ++) total_compressed_size += read_be32_unaligned(context -> data + *chunk - 8) - chunkoffset;
  unsigned char * compressed = ctxmalloc(context, total_compressed_size);
  for (const size_t * chunk = chunks; *chunk; chunk ++) {
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
                                  uint8_t max_palette_index, bool interlaced) {
  // imagetype must be 3 here
  uint8_t * result = ctxmalloc(context, (size_t) width * height);
  unsigned char * decompressed;
  if (interlaced) {
    size_t widths[] = {(width + 7) / 8, (width + 3) / 8, (width + 3) / 4, (width + 1) / 4, (width + 1) / 2, width / 2, width};
    size_t heights[] = {(height + 7) / 8, (height + 7) / 8, (height + 3) / 8, (height + 3) / 4, (height + 1) / 4, (height + 1) / 2, height / 2};
    size_t rowsizes[7];
    size_t cumulative_size = 0;
    for (uint_fast8_t pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      rowsizes[pass] = ((size_t) widths[pass] * bitdepth + 7) / 8 + 1;
      cumulative_size += heights[pass] * rowsizes[pass];
    }
    decompressed = decompress_PNG_data(context, compressed, compressed_size, cumulative_size);
    unsigned char * current = decompressed;
    unsigned char * rowdata = ctxmalloc(context, width);
    for (uint_fast8_t pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      remove_PNG_filter(context, current, widths[pass], heights[pass], 3, bitdepth);
      for (size_t row = 0; row < heights[pass]; row ++) {
        expand_bitpacked_PNG_data(rowdata, current + 1, widths[pass], bitdepth);
        current += rowsizes[pass];
        for (size_t col = 0; col < widths[pass]; col ++)
          result[(row * interlaced_PNG_pass_step[pass] + interlaced_PNG_pass_start[pass]) * width +
                 col * interlaced_PNG_pass_step[pass + 1] + interlaced_PNG_pass_start[pass + 1]] = rowdata[col];
      }
    }
    ctxfree(context, rowdata);
  } else {
    size_t rowsize = ((size_t) width * bitdepth + 7) / 8 + 1;
    decompressed = decompress_PNG_data(context, compressed, compressed_size, rowsize * height);
    remove_PNG_filter(context, decompressed, width, height, 3, bitdepth);
    for (size_t row = 0; row < height; row ++) expand_bitpacked_PNG_data(result + row * width, decompressed + row * rowsize + 1, width, bitdepth);
  }
  ctxfree(context, decompressed);
  for (size_t p = 0; p < (size_t) width * height; p ++) if (result[p] > max_palette_index) throw(context, PLUM_ERR_INVALID_COLOR_INDEX);
  return result;
}

uint64_t * load_PNG_raw_frame (struct context * context, const void * compressed, size_t compressed_size, uint32_t width, uint32_t height, uint8_t imagetype,
                               uint8_t bitdepth, bool interlaced) {
  // imagetype is not 3 here
  uint64_t * result = ctxmalloc(context, sizeof *result * width * height);
  unsigned char * decompressed;
  size_t pixelsize = bitdepth / 8 * channels_per_pixel_PNG[imagetype]; // 0 will be treated as a special value
  if (interlaced) {
    size_t widths[] = {(width + 7) / 8, (width + 3) / 8, (width + 3) / 4, (width + 1) / 4, (width + 1) / 2, width / 2, width};
    size_t heights[] = {(height + 7) / 8, (height + 7) / 8, (height + 3) / 8, (height + 3) / 4, (height + 1) / 4, (height + 1) / 2, height / 2};
    size_t rowsizes[7];
    size_t cumulative_size = 0;
    for (uint_fast8_t pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      rowsizes[pass] = pixelsize ? pixelsize * widths[pass] + 1 : (((size_t) widths[pass] * bitdepth + 7) / 8 + 1);
      cumulative_size += rowsizes[pass] * heights[pass];
    }
    decompressed = decompress_PNG_data(context, compressed, compressed_size, cumulative_size);
    unsigned char * current = decompressed;
    for (uint_fast8_t pass = 0; pass < 7; pass ++) if (widths[pass] && heights[pass]) {
      load_PNG_raw_frame_pass(context, current, result, heights[pass], widths[pass], width, imagetype, bitdepth, interlaced_PNG_pass_start[pass + 1],
                              interlaced_PNG_pass_start[pass], interlaced_PNG_pass_step[pass + 1], interlaced_PNG_pass_step[pass], rowsizes[pass]);
      current += rowsizes[pass] * heights[pass];
    }
  } else {
    size_t rowsize = pixelsize ? pixelsize * width + 1 : (((size_t) width * bitdepth + 7) / 8 + 1);
    decompressed = decompress_PNG_data(context, compressed, compressed_size, rowsize * height);
    load_PNG_raw_frame_pass(context, decompressed, result, height, width, width, imagetype, bitdepth, 0, 0, 1, 1, rowsize);
  }
  ctxfree(context, decompressed);
  return result;
}

void load_PNG_raw_frame_pass (struct context * context, unsigned char * restrict data, uint64_t * restrict output, uint32_t height, uint32_t width,
                              uint32_t fullwidth, uint8_t imagetype, uint8_t bitdepth, unsigned char coordH, unsigned char coordV, unsigned char offsetH,
                              unsigned char offsetV, size_t rowsize) {
  remove_PNG_filter(context, data, width, height, imagetype, bitdepth);
  for (size_t row = 0; row < height; row ++) {
    uint64_t * rowoutput = output + (row * offsetV + coordV) * fullwidth;
    unsigned char * rowdata = data + 1;
    switch (bitdepth + imagetype) {
      // since bitdepth must be 8 or 16 here unless imagetype is 0, all combinations are unique
      case 8: // imagetype = 0, bitdepth = 8
        for (size_t col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) rowdata[col] * 0x10101010101u;
        break;
      case 10: // imagetype = 2, bitdepth = 8
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = (rowdata[3 * col] | ((uint64_t) rowdata[3 * col + 1] << 16) | ((uint64_t) rowdata[3 * col + 2] << 32)) * 0x101;
        break;
      case 12: // imagetype = 4, bitdepth = 8
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = ((uint64_t) rowdata[2 * col] * 0x10101010101u) | ((uint64_t) (rowdata[2 * col + 1] ^ 0xff) * 0x101000000000000u);
        break;
      case 14: // imagetype = 6, bitdepth = 8
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = 0x101 * (rowdata[4 * col] | ((uint64_t) rowdata[4 * col + 1] << 16) |
                                                       ((uint64_t) rowdata[4 * col + 2] << 32) | ((uint64_t) (rowdata[4 * col + 3] ^ 0xff) << 48));
        break;
      case 16: // imagetype = 0, bitdepth = 16
        for (size_t col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) read_be16_unaligned(rowdata + 2 * col) * 0x100010001u;
        break;
      case 18: // imagetype = 2, bitdepth = 16
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = read_be16_unaligned(rowdata + 6 * col) | ((uint64_t) read_be16_unaligned(rowdata + 6 * col + 2) << 16) |
                                              ((uint64_t) read_be16_unaligned(rowdata + 6 * col + 4) << 32);
        break;
      case 20: // imagetype = 4, bitdepth = 16
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = ((uint64_t) read_be16_unaligned(rowdata + 4 * col) * 0x100010001u) |
                                              ((uint64_t) ~read_be16_unaligned(rowdata + 4 * col + 2) << 48);
        break;
      case 22: // imagetype = 6, bitdepth = 16
        for (size_t col = 0; col < width; col ++)
          rowoutput[col * offsetH + coordH] = read_be16_unaligned(rowdata + 8 * col) | ((uint64_t) read_be16_unaligned(rowdata + 8 * col + 2) << 16) |
                                              ((uint64_t) read_be16_unaligned(rowdata + 8 * col + 4) << 32) |
                                              ((uint64_t) ~read_be16_unaligned(rowdata + 8 * col + 6) << 48);
        break;
      default: { // imagetype = 0, bitdepth < 8
        unsigned char * buffer = ctxmalloc(context, width);
        expand_bitpacked_PNG_data(buffer, rowdata, width, bitdepth);
        for (size_t col = 0; col < width; col ++) rowoutput[col * offsetH + coordH] = (uint64_t) bitextend16(buffer[col], bitdepth) * 0x100010001u;
        ctxfree(context, buffer);
      }
    }
    data += rowsize;
  }
}

void expand_bitpacked_PNG_data (unsigned char * restrict result, const unsigned char * restrict source, size_t count, uint8_t bitdepth) {
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
      if (count) for (unsigned char remainder = *source; count; count --, remainder <<= 1) *(result ++) = remainder >> 7;
      break;
    case 2:
      for (; count > 3; count -= 4) {
        *(result ++) = *source >> 6;
        *(result ++) = (*source >> 4) & 3;
        *(result ++) = (*source >> 2) & 3;
        *(result ++) = *(source ++) & 3;
      }
      if (count) for (unsigned char remainder = *source; count; count --, remainder <<= 2) *(result ++) = remainder >> 6;
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
  ptrdiff_t pixelsize = bitdepth / 8 * channels_per_pixel_PNG[imagetype];
  if (!pixelsize) {
    pixelsize = 1;
    width = ((size_t) width * bitdepth + 7) / 8;
  }
  if ((size_t) pixelsize * width + 1 > PTRDIFF_MAX) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  ptrdiff_t rowsize = pixelsize * width + 1;
  for (uint_fast32_t row = 0; row < height; row ++) {
    unsigned char * rowdata = data + 1;
    switch (*data) {
      case 4:
        for (size_t p = 0; p < pixelsize * width; p ++) {
          int top = row ? rowdata[p - rowsize] : 0, left = (p < pixelsize) ? 0 : rowdata[p - pixelsize];
          int diagonal = (row && p >= pixelsize) ? rowdata[p - pixelsize - rowsize] : 0;
          int topdiff = absolute_value(left - diagonal), leftdiff = absolute_value(top - diagonal), diagdiff = absolute_value(left + top - diagonal * 2);
          rowdata[p] += (leftdiff <= topdiff && leftdiff <= diagdiff) ? left : (topdiff <= diagdiff) ? top : diagonal;
        }
        break;
      case 3:
        if (row) {
          for (size_t p = 0; p < pixelsize; p ++) rowdata[p] += rowdata[p - rowsize] >> 1;
          for (size_t p = pixelsize; p < pixelsize * width; p ++) rowdata[p] += (rowdata[p - pixelsize] + rowdata[p - rowsize]) >> 1;
        } else
          for (size_t p = pixelsize; p < pixelsize * width; p ++) rowdata[p] += rowdata[p - pixelsize] >> 1;
        break;
      case 2:
        if (row) for (size_t p = 0; p < pixelsize * width; p ++) rowdata[p] += rowdata[p - rowsize];
        break;
      case 1:
        for (size_t p = pixelsize; p < pixelsize * width; p ++) rowdata[p] += rowdata[p - pixelsize];
      case 0:
        break;
      default:
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    data += rowsize;
  }
}
