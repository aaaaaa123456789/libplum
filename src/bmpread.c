#include "proto.h"

void load_BMP_data (struct context * context, unsigned flags, size_t limit) {
  if (context -> size < 54) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast32_t dataoffset = read_le32_unaligned(context -> data + 10);
  uint_fast32_t subheader = read_le32_unaligned(context -> data + 14);
  if (dataoffset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (dataoffset < (subheader + 14)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (subheader < 40) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  context -> image -> type = PLUM_IMAGE_BMP;
  context -> image -> frames = 1;
  context -> image -> width = read_le32_unaligned(context -> data + 18);
  context -> image -> height = read_le32_unaligned(context -> data + 22);
  if (context -> image -> width > 0x7fffffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  validate_image_size(context, limit);
  int inverted = 1;
  if (context -> image -> height > 0x7fffffffu) {
    context -> image -> height = -context -> image -> height;
    inverted = 0;
  }
  if (read_le16_unaligned(context -> data + 26) != 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast16_t bits = read_le16_unaligned(context -> data + 28);
  uint_fast32_t compression = read_le32_unaligned(context -> data + 30);
  if ((bits > 32) || (compression > 3)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  allocate_framebuffers(context, flags, bits <= 8);
  void * frame;
  uint8_t bitmasks[8];
  uint64_t palette[256];
  switch (bits | (compression << 8)) {
    case 1: // palette-based, first pixel in MSB
      context -> image -> max_palette_index = load_BMP_palette(context, (size_t) 14 + subheader, 2, palette);
      frame = load_monochrome_BMP(context, dataoffset, inverted);
      write_palette_to_image(context, palette, flags);
      write_palette_framebuffer_to_image(context, frame, palette, 0, flags, context -> image -> max_palette_index);
      break;
    case 4: // palette-based, first pixel in upper half
      context -> image -> max_palette_index = load_BMP_palette(context, (size_t) 14 + subheader, 16, palette);
      frame = load_halfbyte_BMP(context, dataoffset, inverted);
      write_palette_to_image(context, palette, flags);
      write_palette_framebuffer_to_image(context, frame, palette, 0, flags, context -> image -> max_palette_index);
      break;
    case 0x204: // 4-bit RLE
      context -> image -> max_palette_index = load_BMP_palette(context, (size_t) 14 + subheader, 16, palette);
      frame = load_halfbyte_compressed_BMP(context, dataoffset, inverted);
      write_palette_to_image(context, palette, flags);
      write_palette_framebuffer_to_image(context, frame, palette, 0, flags, context -> image -> max_palette_index);
      break;
    case 8: // palette-based
      context -> image -> max_palette_index = load_BMP_palette(context, (size_t) 14 + subheader, 256, palette);
      frame = load_byte_BMP(context, dataoffset, inverted);
      write_palette_to_image(context, palette, flags);
      write_palette_framebuffer_to_image(context, frame, palette, 0, flags, context -> image -> max_palette_index);
      break;
    case 0x108: // 8-bit RLE
      context -> image -> max_palette_index = load_BMP_palette(context, (size_t) 14 + subheader, 256, palette);
      frame = load_byte_compressed_BMP(context, dataoffset, inverted);
      write_palette_to_image(context, palette, flags);
      write_palette_framebuffer_to_image(context, frame, palette, 0, flags, context -> image -> max_palette_index);
      break;
    case 16: // mask 0x7c00 red, 0x03e0 green, 0x001f blue
      add_color_depth_metadata(context, 5, 5, 5, 0, 0);
      frame = load_BMP_pixels(context, dataoffset, inverted, 2, &load_BMP_halfword_pixel, (const uint8_t []) {10, 5, 5, 5, 0, 5, 0, 0});
      write_framebuffer_to_image(context -> image, frame, 0, flags);
      break;
    case 0x310: // 16-bit bitfield-based
      load_BMP_bitmasks(context, subheader, bitmasks, 16);
      add_color_depth_metadata(context, bitmasks[1], bitmasks[3], bitmasks[5], bitmasks[7], 0);
      frame = load_BMP_pixels(context, dataoffset, inverted, 2, &load_BMP_halfword_pixel, bitmasks);
      write_framebuffer_to_image(context -> image, frame, 0, flags);
      break;
    case 24: // blue, green, red
      add_color_depth_metadata(context, 8, 8, 8, 0, 0);
      frame = load_BMP_pixels(context, dataoffset, inverted, 3, &load_BMP_RGB_pixel, NULL);
      write_framebuffer_to_image(context -> image, frame, 0, flags);
      break;
    case 32: // blue, green, red, ignored
      add_color_depth_metadata(context, 8, 8, 8, 0, 0);
      frame = load_BMP_pixels(context, dataoffset, inverted, 4, &load_BMP_word_pixel, (const uint8_t []) {16, 8, 8, 8, 0, 8, 0, 0});
      write_framebuffer_to_image(context -> image, frame, 0, flags);
      break;
    case 0x320: // 32-bit bitfield-based
      load_BMP_bitmasks(context, subheader, bitmasks, 32);
      add_color_depth_metadata(context, bitmasks[1], bitmasks[3], bitmasks[5], bitmasks[7], 0);
      frame = load_BMP_pixels(context, dataoffset, inverted, 4, &load_BMP_word_pixel, bitmasks);
      write_framebuffer_to_image(context -> image, frame, 0, flags);
      break;
    default:
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  ctxfree(context, frame);
}

uint8_t load_BMP_palette (struct context * context, size_t offset, unsigned max_count, uint64_t * palette) {
  uint_fast32_t count = read_le32_unaligned(context -> data + 46);
  if (!count || (count > max_count)) count = max_count;
  if ((offset + (count * 4)) > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  unsigned p;
  for (p = 0; p < count; p ++) {
    palette[p] = ((uint64_t) context -> data[offset] << 32) | ((uint64_t) context -> data[offset + 1] << 16) | (uint64_t) context -> data[offset + 2];
    palette[p] *= 0x101;
    offset += 4;
  }
  add_color_depth_metadata(context, 8, 8, 8, 0, 0);
  return count - 1;
}

void load_BMP_bitmasks (struct context * context, size_t headersize, uint8_t * bitmasks, unsigned maxbits) {
  const uint8_t * bp;
  unsigned count;
  if (headersize >= 56) {
    bp = context -> data + 54;
    count = 4;
  } else {
    if (context -> size <= (headersize + 26)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    bp = context -> data + 14 + headersize;
    count = 3;
    bitmasks[6] = bitmasks[7] = 0;
  }
  while (count --) {
    uint_fast32_t mask = read_le32_unaligned(bp);
    *bitmasks = bitmasks[1] = 0;
    if (mask) {
      while (!(mask & 1)) {
        ++ *bitmasks;
        mask >>= 1;
      }
      while (mask & 1) {
        bitmasks[1] ++;
        mask >>= 1;
      }
      if (mask) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (bitmasks[1] > 16) {
        *bitmasks += bitmasks[1] - 16;
        bitmasks[1] = 16;
      }
      if ((*bitmasks + bitmasks[1]) > maxbits) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    bp += 4;
    bitmasks += 2;
  }
}

uint8_t * load_monochrome_BMP (struct context * context, size_t offset, int inverted) {
  size_t rowsize = ((context -> image -> width + 31) >> 3) & ~3u;
  size_t imagesize = rowsize * context -> image -> height;
  if ((offset + imagesize) > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint8_t * frame = ctxmalloc(context, (size_t) context -> image -> width * context -> image -> height);
  const unsigned char * rowdata = context -> data + offset + (inverted ? rowsize * (context -> image -> height - 1) : 0);
  uint_fast32_t row, pos;
  size_t cell = 0;
  for (row = 0; row < context -> image -> height; row ++) {
    const unsigned char * pixeldata = rowdata;
    for (pos = (context -> image -> width >> 3); pos; pos --, pixeldata ++) {
      frame[cell ++] = !!(*pixeldata & 0x80);
      frame[cell ++] = !!(*pixeldata & 0x40);
      frame[cell ++] = !!(*pixeldata & 0x20);
      frame[cell ++] = !!(*pixeldata & 0x10);
      frame[cell ++] = !!(*pixeldata & 8);
      frame[cell ++] = !!(*pixeldata & 4);
      frame[cell ++] = !!(*pixeldata & 2);
      frame[cell ++] = *pixeldata & 1;
    }
    unsigned char remainder = *pixeldata;
    for (pos = context -> image -> width & 7; pos; pos --, remainder <<= 1)
      frame[cell ++] = !!(remainder & 0x80);
    if (inverted)
      rowdata -= rowsize;
    else
      rowdata += rowsize;
  }
  return frame;
}

uint8_t * load_halfbyte_BMP (struct context * context, size_t offset, int inverted) {
  size_t rowsize = ((context -> image -> width + 7) >> 1) & ~3u;
  size_t imagesize = rowsize * context -> image -> height;
  if ((offset + imagesize) > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint8_t * frame = ctxmalloc(context, (size_t) context -> image -> width * context -> image -> height);
  const unsigned char * rowdata = context -> data + offset + (inverted ? rowsize * (context -> image -> height - 1) : 0);
  uint_fast32_t row, pos;
  size_t cell = 0;
  for (row = 0; row < context -> image -> height; row ++) {
    const unsigned char * pixeldata = rowdata;
    for (pos = (context -> image -> width >> 1); pos; pos --) {
      frame[cell ++] = *pixeldata >> 4;
      frame[cell ++] = *(pixeldata ++) & 15;
    }
    if (context -> image -> width & 1) frame[cell ++] = *pixeldata >> 4;
    if (inverted)
      rowdata -= rowsize;
    else
      rowdata += rowsize;
  }
  return frame;
}

uint8_t * load_byte_BMP (struct context * context, size_t offset, int inverted) {
  size_t rowsize = (context -> image -> width + 3) & ~3u;
  size_t imagesize = rowsize * context -> image -> height;
  if ((offset + imagesize) > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint8_t * frame = ctxmalloc(context, (size_t) context -> image -> width * context -> image -> height);
  uint_fast32_t row;
  if (inverted || (context -> image -> width & 3))
    for (row = 0; row < context -> image -> height; row ++)
      memcpy(frame + context -> image -> width * row,
             context -> data + offset + rowsize * (inverted ? context -> image -> height - 1 - row : row),
             context -> image -> width);
  else
    memcpy(frame, context -> data + offset, imagesize);
  return frame;
}

uint8_t * load_halfbyte_compressed_BMP (struct context * context, size_t offset, int inverted) {
  if (!inverted) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  const unsigned char * data = context -> data + offset;
  size_t remaining = context -> size - offset;
  uint8_t * frame = ctxcalloc(context, (size_t) context -> image -> width * context -> image -> height);
  uint_fast32_t row = context -> image -> height - 1, col = 0;
  while (remaining >= 2) {
    unsigned char length = *(data ++);
    unsigned char databyte = *(data ++);
    remaining -= 2;
    if (length) {
      if ((col + length) > context -> image -> width) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      uint8_t * framedata = frame + (size_t) row * context -> image -> width + col;
      col += length;
      while (length) {
        *(framedata ++) = databyte >> 4;
        databyte = (databyte >> 4) | (databyte << 4);
        length --;
      }
    } else switch (databyte) {
      case 0:
        if (!row) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        row --;
        col = 0;
        break;
      case 1:
        return frame;
      case 2:
        if ((remaining < 2) || ((col + *data) > context -> image -> width) || (data[1] > row))
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        col += *(data ++);
        row -= *(data ++);
        remaining -= 2;
        break;
      default: {
        if ((col + databyte) > context -> image -> width) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (remaining < (((databyte + 3) & ~3u) >> 1)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        uint_fast8_t pos;
        uint8_t * framedata = frame + (size_t) row * context -> image -> width + col;
        for (pos = 0; pos < (databyte >> 1); pos ++) {
          *(framedata ++) = data[pos] >> 4;
          *(framedata ++) = data[pos] & 15;
        }
        if (databyte & 1) *framedata = data[pos] >> 4;
        col += databyte;
        data += ((databyte + 3) & ~3u) >> 1;
        remaining -= ((databyte + 3) & ~3u) >> 1;
      }
    }
  }
  throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
}

uint8_t * load_byte_compressed_BMP (struct context * context, size_t offset, int inverted) {
  if (!inverted) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  const unsigned char * data = context -> data + offset;
  size_t remaining = context -> size - offset;
  uint8_t * frame = ctxcalloc(context, (size_t) context -> image -> width * context -> image -> height);
  uint_fast32_t row = context -> image -> height - 1, col = 0;
  while (remaining >= 2) {
    unsigned char length = *(data ++);
    unsigned char databyte = *(data ++);
    remaining -= 2;
    if (length) {
      if ((col + length) > context -> image -> width) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      memset(frame + (size_t) row * context -> image -> width + col, databyte, length);
      col += length;
    } else switch (databyte) {
      case 0:
        if (!row) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        row --;
        col = 0;
        break;
      case 1:
        return frame;
      case 2:
        if ((remaining < 2) || ((col + *data) > context -> image -> width) || (data[1] > row))
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        col += *(data ++);
        row -= *(data ++);
        remaining -= 2;
        break;
      default:
        if ((col + databyte) > context -> image -> width) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (remaining < ((1 + (size_t) databyte) & ~1)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        memcpy(frame + (size_t) row * context -> image -> width + col, data, databyte);
        col += databyte;
        data += (databyte + 1) & ~1;
        remaining -= (databyte + 1) & ~1;
    }
  }
  throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
}

uint64_t * load_BMP_pixels (struct context * context, size_t offset, int inverted, unsigned bytes,
                            uint64_t (* loader) (const unsigned char *, const void *), const void * loaderdata) {
  size_t rowsize = (context -> image -> width * bytes + 3) & ~3u;
  size_t imagesize = rowsize * context -> image -> height;
  if ((offset + imagesize) > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  const unsigned char * rowdata = context -> data + offset + (inverted ? rowsize * (context -> image -> height - 1) : 0);
  uint_fast32_t row, col;
  size_t cell = 0;
  uint64_t * frame = ctxmalloc(context, (size_t) context -> image -> height * context -> image -> width * sizeof *frame);
  for (row = 0; row < context -> image -> height; row ++) {
    const unsigned char * pixeldata = rowdata;
    for (col = 0; col < context -> image -> width; col ++) {
      frame[cell ++] = loader(pixeldata, loaderdata);
      pixeldata += bytes;
    }
    if (inverted)
      rowdata -= rowsize;
    else
      rowdata += rowsize;
  }
  return frame;
}

uint64_t load_BMP_halfword_pixel (const unsigned char * data, const void * bitmasks) {
  return load_BMP_bitmasked_pixel(read_le16_unaligned(data), bitmasks);
}

uint64_t load_BMP_word_pixel (const unsigned char * data, const void * bitmasks) {
  return load_BMP_bitmasked_pixel(read_le32_unaligned(data), bitmasks);
}

uint64_t load_BMP_RGB_pixel (const unsigned char * data, const void * bitmasks) {
  (void)bitmasks;
  return (((uint64_t) *data << 32) | ((uint64_t) data[1] << 16) | (uint64_t) data[2]) * 0x101;
}

uint64_t load_BMP_bitmasked_pixel (uint_fast32_t pixel, const uint8_t * bitmasks) {
  uint64_t result = 0;
  if (bitmasks[1]) result |= bitextend((pixel >> *bitmasks) & (((uint_fast32_t) 1 << bitmasks[1]) - 1), bitmasks[1]);
  if (bitmasks[3]) result |= (uint64_t) bitextend((pixel >> bitmasks[2]) & (((uint_fast32_t) 1 << bitmasks[3]) - 1), bitmasks[3]) << 16;
  if (bitmasks[5]) result |= (uint64_t) bitextend((pixel >> bitmasks[4]) & (((uint_fast32_t) 1 << bitmasks[5]) - 1), bitmasks[5]) << 32;
  if (bitmasks[7]) result |= (~(uint64_t) bitextend((pixel >> bitmasks[6]) & (((uint_fast32_t) 1 << bitmasks[7]) - 1), bitmasks[7])) << 48;
  return result;
}
