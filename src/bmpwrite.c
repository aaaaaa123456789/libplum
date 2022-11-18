#include "proto.h"

void generate_BMP_data (struct context * context) {
  if (context -> source -> frames > 1) throw(context, PLUM_ERR_NO_MULTI_FRAME);
  if (context -> source -> width > 0x7fffffffu || context -> source -> height > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  unsigned char * header = append_output_node(context, 14);
  bytewrite(header, 0x42, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  uint32_t depth = get_true_color_depth(context -> source);
  if (depth >= 0x1000000u)
    generate_BMP_bitmasked_data(context, depth, header + 10);
  else if (context -> source -> palette)
    if (context -> source -> max_palette_index < 16)
      generate_BMP_palette_halfbyte_data(context, header + 10);
    else
      generate_BMP_palette_byte_data(context, header + 10);
  else if (bit_depth_less_than(depth, 0x80808u))
    generate_BMP_RGB_data(context, header + 10);
  else
    generate_BMP_bitmasked_data(context, depth, header + 10);
  size_t filesize = get_total_output_size(context);
  if (filesize <= 0x7fffffffu) write_le32_unaligned(header + 2, filesize);
}

void generate_BMP_bitmasked_data (struct context * context, uint32_t depth, unsigned char * offset_pointer) {
  uint8_t reddepth = depth, greendepth = depth >> 8, bluedepth = depth >> 16, alphadepth = depth >> 24;
  uint_fast8_t totaldepth = reddepth + greendepth + bluedepth + alphadepth;
  if (totaldepth > 32) {
    reddepth = ((reddepth << 6) / totaldepth + 1) >> 1;
    greendepth = ((greendepth << 6) / totaldepth + 1) >> 1;
    bluedepth = ((bluedepth << 6) / totaldepth + 1) >> 1;
    alphadepth = ((alphadepth << 6) / totaldepth + 1) >> 1;
    totaldepth = reddepth + greendepth + bluedepth + alphadepth;
    while (totaldepth > 32) {
      if (alphadepth > 2) totaldepth --, alphadepth --;
      if (bluedepth > 2 && totaldepth > 32) totaldepth --, bluedepth --;
      if (reddepth > 2 && totaldepth > 32) totaldepth --, reddepth --;
      if (greendepth > 2 && totaldepth > 32) totaldepth --, greendepth --;
    }
  }
  if (totaldepth > 16 && totaldepth < 32 && alphadepth == 1) {
    alphadepth += 32 - totaldepth;
    totaldepth = 32;
  }
  uint8_t blueshift = reddepth + greendepth, alphashift = blueshift + bluedepth;
  unsigned char * attributes = append_output_node(context, 108);
  memset(attributes, 0, 108);
  write_le32_unaligned(offset_pointer, 122);
  *attributes = 108;
  write_le32_unaligned(attributes + 4, context -> source -> width);
  write_le32_unaligned(attributes + 8, context -> source -> height);
  attributes[12] = 1;
  attributes[14] = (totaldepth <= 16) ? 16 : 32;
  attributes[16] = 3;
  write_le32_unaligned(attributes + 40, ((uint32_t) 1 << reddepth) - 1);
  write_le32_unaligned(attributes + 44, (((uint32_t) 1 << greendepth) - 1) << reddepth);
  write_le32_unaligned(attributes + 48, (((uint32_t) 1 << bluedepth) - 1) << blueshift);
  write_le32_unaligned(attributes + 52, alphadepth ? (((uint32_t) 1 << alphadepth) - 1) << alphashift : 0);
  write_le32_unaligned(attributes + 56, 0x73524742u); // 'sRGB'
  size_t rowsize = (size_t) context -> source -> width * (attributes[14] >> 3);
  if (totaldepth <= 16 && (context -> source -> width & 1)) rowsize += 2;
  size_t imagesize = rowsize * context -> source -> height;
  if (imagesize <= 0x7fffffffu) write_le32_unaligned(attributes + 20, imagesize);
  unsigned char * data = append_output_node(context, imagesize);
  uint_fast32_t row = context -> source -> height - 1;
  do {
    size_t pos = (size_t) row * context -> source -> width;
    for (uint_fast32_t p = 0; p < context -> source -> width; p ++) {
      uint64_t color;
      const void * colordata = context -> source -> data;
      size_t index = pos ++;
      if (context -> source -> palette) {
        index = context -> source -> data8[index];
        colordata = context -> source -> palette;
      }
      switch (context -> source -> color_format & PLUM_COLOR_MASK) {
        case PLUM_COLOR_16: color = index[(const uint16_t *) colordata]; break;
        case PLUM_COLOR_64: color = index[(const uint64_t *) colordata]; break;
        default: color = index[(const uint32_t *) colordata];
      }
      color = plum_convert_color(color, context -> source -> color_format, PLUM_COLOR_64 | PLUM_ALPHA_INVERT);
      uint_fast32_t out = ((color & 0xffffu) >> (16 - reddepth)) | ((color & 0xffff0000u) >> (32 - greendepth) << reddepth) |
                          ((color & 0xffff00000000u) >> (48 - bluedepth) << blueshift);
      if (alphadepth) out |= color >> (64 - alphadepth) << alphashift;
      if (totaldepth <= 16) {
        write_le16_unaligned(data, out);
        data += 2;
      } else {
        write_le32_unaligned(data, out);
        data += 4;
      }
    }
    if (totaldepth <= 16 && (context -> source -> width & 1)) data += byteappend(data, 0x00, 0x00);
  } while (row --);
}

void generate_BMP_palette_halfbyte_data (struct context * context, unsigned char * offset_pointer) {
  unsigned char * attributes = append_output_node(context, 40);
  write_le32_unaligned(offset_pointer, 58 + 4 * context -> source -> max_palette_index);
  memset(attributes, 0, 40);
  *attributes = 40;
  write_le32_unaligned(attributes + 4, context -> source -> width);
  write_le32_unaligned(attributes + 8, context -> source -> height);
  attributes[12] = 1;
  attributes[14] = 4;
  write_le32_unaligned(attributes + 32, context -> source -> max_palette_index + 1);
  append_BMP_palette(context);
  size_t rowsize = ((context -> source -> width + 7) & ~7u) >> 1;
  if (context -> source -> max_palette_index < 2) rowsize = ((rowsize >> 2) + 3) & bitnegate(3);
  size_t imagesize = rowsize * context -> source -> height;
  unsigned char * data = append_output_node(context, imagesize);
  size_t compressed = try_compress_BMP(context, imagesize, &compress_BMP_halfbyte_row);
  if (compressed) {
    attributes[16] = 2;
    if (compressed <= 0x7fffffffu) write_le32_unaligned(attributes + 20, compressed);
    context -> output -> size = compressed;
    return;
  }
  uint_fast32_t row = context -> source -> height - 1;
  uint_fast8_t padding = 3u & ~((context -> source -> width - 1) >> ((context -> source -> max_palette_index < 2) ? 3 : 1));
  do {
    const uint8_t * source = context -> source -> data8 + (size_t) row * context -> source -> width;
    if (context -> source -> max_palette_index < 2) {
      uint_fast8_t value = 0;
      for (uint_fast32_t p = 0; p < context -> source -> width; p ++) {
        value = (value << 1) | source[p];
        if ((p & 7) == 7) {
          *(data ++) = value;
          value = 0;
        }
      }
      if (context -> source -> width & 7) *(data ++) = value << (8 - (context -> source -> width & 7));
      attributes[14] = 1;
    } else {
      for (uint_fast32_t p = 0; p < context -> source -> width - 1; p += 2) *(data ++) = (source[p] << 4) | source[p + 1];
      if (context -> source -> width & 1) *(data ++) = source[context -> source -> width - 1] << 4;
    }
    for (uint_fast8_t value = 0; value < padding; value ++) *(data ++) = 0;
  } while (row --);
}

void generate_BMP_palette_byte_data (struct context * context, unsigned char * offset_pointer) {
  unsigned char * attributes = append_output_node(context, 40);
  write_le32_unaligned(offset_pointer, 58 + 4 * context -> source -> max_palette_index);
  memset(attributes, 0, 40);
  *attributes = 40;
  write_le32_unaligned(attributes + 4, context -> source -> width);
  write_le32_unaligned(attributes + 8, context -> source -> height);
  attributes[12] = 1;
  attributes[14] = 8;
  write_le32_unaligned(attributes + 32, context -> source -> max_palette_index + 1);
  append_BMP_palette(context);
  size_t rowsize = (context -> source -> width + 3) & bitnegate(3), imagesize = rowsize * context -> source -> height;
  unsigned char * data = append_output_node(context, imagesize);
  size_t compressed = try_compress_BMP(context, imagesize, &compress_BMP_byte_row);
  if (compressed) {
    attributes[16] = 1;
    if (compressed <= 0x7fffffffu) write_le32_unaligned(attributes + 20, compressed);
    context -> output -> size = compressed;
    return;
  }
  uint_fast32_t row = context -> source -> height - 1;
  do {
    memcpy(data, context -> source -> data8 + row * context -> source -> width, context -> source -> width);
    if (rowsize != context -> source -> width) memset(data + context -> source -> width, 0, rowsize - context -> source -> width);
    data += rowsize;
  } while (row --);
}

size_t try_compress_BMP (struct context * context, size_t size, size_t (* rowhandler) (uint8_t * restrict, const uint8_t * restrict, size_t)) {
  uint8_t * rowdata = ctxmalloc(context, size * ((context -> source -> max_palette_index < 2) ? 8 : 2) + 2);
  uint8_t * output = context -> output -> data;
  size_t cumulative = 0;
  uint_fast32_t row = context -> source -> height - 1;
  do {
    size_t rowsize = rowhandler(rowdata, context -> source -> data8 + (size_t) row * context -> source -> width, context -> source -> width);
    cumulative += rowsize;
    if (cumulative >= size) {
      ctxfree(context, rowdata);
      return 0;
    }
    if (!row) rowdata[rowsize - 1] = 1; // convert a 0x00, 0x00 (EOL) into 0x00, 0x01 (EOF)
    memcpy(output, rowdata, rowsize);
    output += rowsize;
  } while (row --);
  ctxfree(context, rowdata);
  return cumulative;
}

size_t compress_BMP_halfbyte_row (uint8_t * restrict result, const uint8_t * restrict data, size_t count) {
  size_t size = 2; // account for the terminator
  while (count > 3)
    if (*data == data[2] && data[1] == data[3]) {
      uint_fast8_t length;
      for (length = 4; length < 0xff && length < count && data[length] == data[length - 2]; length ++);
      result += byteappend(result, length, (*data << 4) | data[1]);
      size += 2;
      data += length;
      count -= length;
    } else {
      size_t length;
      uint_fast8_t matches = 0;
      for (length = 2; length < count; length ++) {
        if (data[length] == data[length - 2])
          matches ++;
        else
          matches = 0;
        if (matches >= 5) {
          length -= matches;
          break;
        }
      }
      while (length > 2) {
        uint_fast8_t block = (length > 0xff) ? 0xfc : length;
        result += byteappend(result, 0, block);
        size += (block + 7) >> 2 << 1;
        length -= block;
        count -= block;
        while (block >= 4) {
          result += byteappend(result, (*data << 4) | data[1], (data[2] << 4) | data[3]);
          data += 4;
          block -= 4;
        }
        switch (block) {
          case 1: result += byteappend(result, *data << 4, 0); break;
          case 2: result += byteappend(result, (*data << 4) | data[1], 0); break;
          case 3: result += byteappend(result, (*data << 4) | data[1], data[2] << 4);
        }
        data += block;
      }
      matches = emit_BMP_compressed_halfbyte_remainder(result, data, length);
      result += matches;
      size += matches;
      data += length;
      count -= length;
    }
  count = emit_BMP_compressed_halfbyte_remainder(result, data, count);
  result[count] = result[count + 1] = 0;
  return size + count;
}

unsigned emit_BMP_compressed_halfbyte_remainder (uint8_t * restrict result, const uint8_t * restrict data, unsigned count) {
  switch (count) {
    case 1:
      bytewrite(result, 1, *data << 4);
      return 2;
    case 2:
      bytewrite(result, 2, (*data << 4) | data[1]);
      return 2;
    case 3:
      result += byteappend(result, 2 + (*data == data[2]), (*data << 4) | data[1]);
      if (*data == data[2]) return 2;
      bytewrite(result, 1, data[2] << 4);
      return 4;
    default:
      return 0;
  }
}

size_t compress_BMP_byte_row (uint8_t * restrict result, const uint8_t * restrict data, size_t count) {
  size_t size = 2; // account for the terminator
  while (count > 1)
    if (*data == data[1]) {
      uint_fast8_t length;
      for (length = 2; length < 0xff && length < count && *data == data[length]; length ++);
      result += byteappend(result, length, *data);
      size += 2;
      data += length;
      count -= length;
    } else {
      size_t length;
      uint_fast8_t matches = 0;
      for (length = 1; length < count; length ++) {
        if (data[length] == data[length - 1])
          matches ++;
        else
          matches = 0;
        if (matches >= 2) {
          length -= matches;
          break;
        }
      }
      while (length > 2) {
        uint_fast8_t block = (length > 0xff) ? 0xfe : length;
        result += byteappend(result, 0, block);
        memcpy(result, data, block);
        result += block;
        data += block;
        size += 2 + block;
        length -= block;
        count -= block;
        if (block & 1) {
          *(result ++) = 0;
          size ++;
        }
      }
      if (length == 2) {
        matches = 1 + (*data == data[1]);
        result += byteappend(result, matches, *data);
        size += 2;
        data += matches;
        count -= matches;
        length -= matches;
      }
      if (length == 1) {
        result += byteappend(result, 1, *data);
        data ++;
        size += 2;
        count --;
      }
    }
  if (count == 1) {
    result += byteappend(result, 1, *data);
    size += 2;
  }
  bytewrite(result, 0, 0);
  return size;
}

void append_BMP_palette (struct context * context) {
  unsigned char * data = append_output_node(context, 4 * (context -> source -> max_palette_index + 1));
  uint32_t * colors = ctxmalloc(context, sizeof *colors * (context -> source -> max_palette_index + 1));
  plum_convert_colors(colors, context -> source -> palette, context -> source -> max_palette_index + 1, PLUM_COLOR_32, context -> source -> color_format);
  for (unsigned p = 0; p <= context -> source -> max_palette_index; p ++) data += byteappend(data, colors[p] >> 16, colors[p] >> 8, colors[p], 0);
  ctxfree(context, colors);
}

void generate_BMP_RGB_data (struct context * context, unsigned char * offset_pointer) {
  unsigned char * attributes = append_output_node(context, 40);
  write_le32_unaligned(offset_pointer, 54);
  memset(attributes, 0, 40);
  *attributes = 40;
  write_le32_unaligned(attributes + 4, context -> source -> width);
  write_le32_unaligned(attributes + 8, context -> source -> height);
  attributes[12] = 1;
  attributes[14] = 24;
  uint32_t * data;
  if ((context -> source -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32)
    data = context -> source -> data;
  else {
    data = ctxmalloc(context, sizeof *data * context -> source -> width * context -> source -> height);
    plum_convert_colors(data, context -> source -> data, (size_t) context -> source -> width * context -> source -> height,
                        PLUM_COLOR_32, context -> source -> color_format);
  }
  size_t rowsize = (size_t) context -> source -> width * 3, padding = 0;
  if (rowsize & 3) {
    padding = 4 - (rowsize & 3);
    rowsize += padding;
  }
  unsigned char * out = append_output_node(context, rowsize * context -> source -> height);
  uint_fast32_t row = context -> source -> height - 1;
  do {
    size_t pos = (size_t) row * context -> source -> width;
    for (uint_fast32_t remaining = context -> source -> width; remaining; pos ++, remaining --)
      out += byteappend(out, data[pos] >> 16, data[pos] >> 8, data[pos]);
    for (uint_fast32_t p = 0; p < padding; p ++) *(out ++) = 0;
  } while (row --);
  if (data != context -> source -> data) ctxfree(context, data);
}
