#include "proto.h"

void load_PNM_data (struct context * context, unsigned flags) {
  struct PNM_image_header * headers = NULL;
  size_t offset = 0;
  context -> image -> type = PLUM_IMAGE_PNM;
  // all image fields are zero-initialized, so the sizes are set to 0
  while (offset < context -> size) {
    if (context -> image -> frames == 0xffffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
    headers = ctxrealloc(context, headers, (context -> image -> frames + 1) * sizeof *headers);
    struct PNM_image_header * header = headers + (context -> image -> frames ++);
    load_PNM_header(context, offset, header);
    if ((context -> size - header -> datastart) < header -> datalength) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (header -> width > context -> image -> width) context -> image -> width = header -> width;
    if (header -> height > context -> image -> height) context -> image -> height = header -> height;
    offset = header -> datastart + header -> datalength;
    skip_PNM_whitespace(context, &offset);
  }
  allocate_framebuffers(context, flags, 0);
  add_PNM_bit_depth_metadata(context, headers);
  uint64_t * buffer = ctxmalloc(context, sizeof *buffer * context -> image -> width * context -> image -> height);
  uint_fast32_t frame;
  offset = plum_color_buffer_size((size_t) context -> image -> width * context -> image -> height, flags);
  for (frame = 0; frame < context -> image -> frames; frame ++) {
    load_PNM_frame(context, headers + frame, buffer);
    plum_convert_colors(context -> image -> data8 + offset * frame, buffer, (size_t) context -> image -> width * context -> image -> height, flags,
                        PLUM_COLOR_64 | PLUM_ALPHA_INVERT);
  }
  ctxfree(context, buffer);
  ctxfree(context, headers);
}

void load_PNM_header (struct context * context, size_t offset, struct PNM_image_header * restrict header) {
  if ((context -> size - offset) < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (bytematch(context -> data, 0xef, 0xbb, 0xbf)) offset += 3; // if a broken text editor somehow inserted a UTF-8 BOM, skip it
  if (context -> data[offset ++] != 0x50) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  header -> type = context -> data[offset ++] - 0x30;
  if (!header -> type || (header -> type > 7) || !is_whitespace(context -> data[offset])) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (header -> type == 7) {
    load_PAM_header(context, offset, header);
    return;
  }
  uint32_t dimensions[3];
  dimensions[2] = 1;
  read_PNM_numbers(context, &offset, dimensions, 2 + ((header -> type != 1) && (header -> type != 4)));
  if (!(*dimensions && dimensions[1] && dimensions[2]) || (dimensions[2] > 0xffffu) || (offset == context -> size)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  header -> width = *dimensions;
  header -> height = dimensions[1];
  header -> maxvalue = dimensions[2];
  header -> datastart = ++ offset;
  if (!plum_check_valid_image_size(header -> width, header -> height, 1)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  switch (header -> type) {
    case 5: case 6:
      header -> datalength = (size_t) header -> width * header -> height * (1 + (header -> maxvalue > 0xff));
      if (header -> type == 6) header -> datalength *= 3;
      break;
    case 4:
      header -> datalength = (size_t) (((header -> width - 1) >> 3) + 1) * header -> height;
      break;
    default:
      header -> datalength = context -> size - offset;
      if (header -> datalength < (header -> type[(const size_t []) {1, 1, 2, 6}] * header -> width * header -> height - 1))
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void load_PAM_header (struct context * context, size_t offset, struct PNM_image_header * restrict header) {
  int fields = 15; // bits 0-3: width, height, max, depth
  uint32_t value, depth;
  while (1) {
    skip_PNM_line(context, &offset);
    skip_PNM_whitespace(context, &offset);
    unsigned length = next_PNM_token_length(context, offset);
    if ((length == 6) && bytematch(context -> data + offset, 0x45, 0x4e, 0x44, 0x48, 0x44, 0x52)) { // ENDHDR
      offset += 6;
      break;
    } else if ((length == 5) && bytematch(context -> data + offset, 0x57, 0x49, 0x44, 0x54, 0x48)) { // WIDTH
      offset += 5;
      if (!(fields & 1)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      read_PNM_numbers(context, &offset, &value, 1);
      if (!value) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      header -> width = value;
      fields &= ~1;
    } else if ((length == 6) && bytematch(context -> data + offset, 0x48, 0x45, 0x49, 0x47, 0x48, 0x54)) { // HEIGHT
      offset += 6;
      if (!(fields & 2)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      read_PNM_numbers(context, &offset, &value, 1);
      if (!value) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      header -> height = value;
      fields &= ~2;
    } else if ((length == 6) && bytematch(context -> data + offset, 0x4d, 0x41, 0x58, 0x56, 0x41, 0x4c)) { // MAXVAL
      offset += 6;
      if (!(fields & 4)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      read_PNM_numbers(context, &offset, &value, 1);
      if (!value || (value > 0xffffu)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      header -> maxvalue = value;
      fields &= ~4;
    } else if ((length == 5) && bytematch(context -> data + offset, 0x44, 0x45, 0x50, 0x54, 0x48)) { // DEPTH
      offset += 5;
      if (!(fields & 8)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      read_PNM_numbers(context, &offset, &depth, 1);
      fields &= ~8;
    } else if ((length == 8) && bytematch(context -> data + offset, 0x54, 0x55, 0x50, 0x4c, 0x54, 0x59, 0x50, 0x45)) { // TUPLTYPE
      if (header -> type != 7) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      offset += 8;
      skip_PNM_whitespace(context, &offset);
      // while the TUPLTYPE line is, by the spec, not tokenized, the only recognized tuple types are a single token
      length = next_PNM_token_length(context, offset);
      if ((length == 13) && bytematch(context -> data + offset, 0x42, 0x4c, 0x41, 0x43, 0x4b, 0x41, 0x4e, 0x44, 0x57, 0x48, 0x49, 0x54, 0x45)) // BLACKANDWHITE
        header -> type = 11;
      else if ((length == 9) && bytematch(context -> data + offset, 0x47, 0x52, 0x41, 0x59, 0x53, 0x43, 0x41, 0x4c, 0x45)) // GRAYSCALE
        header -> type = 12;
      else if ((length == 3) && bytematch(context -> data + offset, 0x52, 0x47, 0x42)) // RGB
        header -> type = 13;
      else if ((length == 19) && bytematch(context -> data + offset, 0x42, 0x4c, 0x41, 0x43, 0x4b, 0x41, 0x4e, 0x44, 0x57, 0x48,
                                                                     0x49, 0x54, 0x45, 0x5f, 0x41, 0x4c, 0x50, 0x48, 0x41)) // BLACKANDWHITE_ALPHA
        header -> type = 14;
      else if ((length == 15) && bytematch(context -> data + offset, 0x47, 0x52, 0x41, 0x59, 0x53, 0x43, 0x41, 0x4c, 0x45, 0x5f,
                                                                     0x41, 0x4c, 0x50, 0x48, 0x41)) // GRAYSCALE_ALPHA
        header -> type = 15;
      else if ((length == 9) && bytematch(context -> data + offset, 0x52, 0x47, 0x42, 0x5f, 0x41, 0x4c, 0x50, 0x48, 0x41)) // RGB_ALPHA
        header -> type = 16;
      else
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      offset += length;
    } else
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  if (fields || (header -> type == 7)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!plum_check_valid_image_size(header -> width, header -> height, 1)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  value = header -> type - 11;
  if (depth != value[(const uint32_t []) {1, 1, 3, 2, 2, 4}]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if ((header -> maxvalue != 1) && ((header -> type == 11) || (header -> type == 14))) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  skip_PNM_line(context, &offset);
  header -> datastart = offset;
  header -> datalength = (size_t) header -> width * header -> height * depth;
  if (header -> maxvalue > 0xff) header -> datalength *= 2;
}

void skip_PNM_whitespace (struct context * context, size_t * restrict offset) {
  while (*offset < context -> size)
    if (context -> data[*offset] == 0x23) // '#'
      while ((*offset < context -> size) && (context -> data[*offset] != 10)) ++ *offset;
    else if (is_whitespace(context -> data[*offset]))
      ++ *offset;
    else
      break;
}

void skip_PNM_line (struct context * context, size_t * restrict offset) {
  int comment;
  for (comment = 0; (*offset < context -> size) && (context -> data[*offset] != 10); ++ *offset) if (!comment)
    if (context -> data[*offset] == 0x23) // '#'
      comment = 1;
    else if (!is_whitespace(context -> data[*offset]))
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (*offset < context -> size) ++ *offset;
}

unsigned next_PNM_token_length (struct context * context, size_t offset) {
  // stops at 20 because the longest recognized token is 19 characters long
  unsigned result = 0;
  while ((offset < context -> size) && (result < 20) && !is_whitespace(context -> data[offset])) result ++, offset ++;
  return (result >= 20) ? 0 : result;
}

void read_PNM_numbers (struct context * context, size_t * restrict offset, uint32_t * restrict result, size_t count) {
  while (count --) {
    skip_PNM_whitespace(context, offset);
    uint64_t current = 0; // 64-bit so it can catch overflows
    if ((*offset >= context -> size) || (context -> data[*offset] < 0x30) || (context -> data[*offset] > 0x39)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    while ((*offset < context -> size) && (context -> data[*offset] >= 0x30) && (context -> data[*offset] <= 0x39)) {
      current = current * 10 + context -> data[(*offset) ++] - 0x30;
      if (current > 0xffffffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    if ((*offset < context -> size) && !is_whitespace(context -> data[*offset])) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    *(result ++) = current;
  }
}

void add_PNM_bit_depth_metadata (struct context * context, const struct PNM_image_header * headers) {
  uint_fast8_t depth, colordepth = 0, alphadepth = 0, colored = 0;
  uint_fast32_t frame;
  for (frame = 0; frame < context -> image -> frames; frame ++) {
    depth = bit_width(headers[frame].maxvalue);
    if ((headers[frame].type == 3) || (headers[frame].type == 6) || (headers[frame].type == 13) || (headers[frame].type == 16)) colored = 1;
    if (colordepth < depth) colordepth = depth;
    if ((headers[frame].type >= 14) && (alphadepth < depth)) alphadepth = depth;
  }
  if (colored)
    add_color_depth_metadata(context, colordepth, colordepth, colordepth, alphadepth, 0);
  else
    add_color_depth_metadata(context, 0, 0, 0, alphadepth, colordepth);
}

void load_PNM_frame (struct context * context, const struct PNM_image_header * header, uint64_t * restrict buffer) {
  size_t p, offset = header -> datastart;
  uint_fast32_t row, col;
  if (header -> width < context -> image -> width)
    for (row = 0; row < header -> height; row ++)
      for (col = header -> width, p = (size_t) context -> image -> width * row + col; col < context -> image -> width; col ++, p ++) buffer[p] = 0;
  if (header -> height < context -> image -> height)
    for (p = (size_t) header -> height * context -> image -> width; p < ((size_t) context -> image -> width * context -> image -> height); p ++) buffer[p] = 0;
  if (header -> type == 4) {
    load_PNM_bit_frame(context, header -> width, header -> height, offset, buffer);
    return;
  }
  uint32_t values[4];
  values[3] = header -> maxvalue;
  uint_fast8_t color, bits = bit_width(header -> maxvalue);
  if (((header -> maxvalue + 1) >> (bits - 1)) == 1) bits = 0; // check if header -> maxvalue isn't (1 << bits) - 1, avoiding UB
  for (row = 0; row < header -> height; row ++) for (col = 0, p = (size_t) context -> image -> width * row; col < header -> width; col ++, p ++) {
    switch (header -> type) {
      case 1:
        // sometimes the 0s and 1s are not delimited at all here, so it needs a special parser
        while ((offset < (context -> size)) && ((context -> data[offset] & ~1) != 0x30)) offset ++;
        if (offset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        values[2] = values[1] = *values = ~context -> data[offset ++] & 1;
        break;
      case 2:
        read_PNM_numbers(context, &offset, values, 1);
        values[2] = values[1] = *values;
        break;
      case 3:
        read_PNM_numbers(context, &offset, values, 3);
        break;
      case 6: case 13: case 16:
        if (header -> maxvalue > 0xff) {
          *values = read_be16_unaligned(context -> data + offset);
          values[1] = read_be16_unaligned(context -> data + (offset += 2));
          values[2] = read_be16_unaligned(context -> data + (offset += 2));
          if (header -> type >= 14) values[3] = read_be16_unaligned(context -> data + (offset += 2));
          offset += 2;
        } else {
          *values = context -> data[offset ++];
          values[1] = context -> data[offset ++];
          values[2] = context -> data[offset ++];
          if (header -> type >= 14) values[3] = context -> data[offset ++];
        }
        break;
      default:
        if (header -> maxvalue > 0xff) {
          *values = read_be16_unaligned(context -> data + offset);
          if (header -> type >= 14) values[3] = read_be16_unaligned(context -> data + (offset += 2));
          offset += 2;
        } else {
          *values = context -> data[offset ++];
          if (header -> type >= 14) values[3] = context -> data[offset ++];
        }
        values[2] = values[1] = *values;
    }
    buffer[p] = 0;
    for (color = 0; color < 4; color ++) {
      uint64_t converted;
      if (bits)
        converted = bitextend(values[color], bits);
      else
        converted = (values[color] * 0xffffu + (header -> maxvalue >> 1)) / header -> maxvalue;
      buffer[p] |= converted << (color * 16);
    }
  }
}

void load_PNM_bit_frame (struct context * context, size_t width, size_t height, size_t offset, uint64_t * restrict buffer) {
  uint_fast32_t row, col;
  for (row = 0; row < height; row ++) {
    uint_fast8_t value, bit;
    size_t p = (size_t) context -> image -> width * row;
    for (col = 0; col < (width & ~7); col += 8) {
      value = context -> data[offset ++];
      for (bit = 0; bit < 8; bit ++) {
        buffer[p ++] = (value & 0x80) ? 0xffff000000000000u : 0xffffffffffffffffu;
        value <<= 1;
      }
    }
    if (width & 7) {
      value = context -> data[offset ++];
      for (bit = 0; bit < (width & 7); bit ++) {
        buffer[p ++] = (value & 0x80) ? 0xffff000000000000u : 0xffffffffffffffffu;
        value <<= 1;
      }
    }
  }
}
