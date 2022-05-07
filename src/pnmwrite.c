#include "proto.h"

void generate_PNM_data (struct context * context) {
  uint32_t depth = get_true_color_depth(context -> source);
  uint_fast8_t max = 0;
  for (uint_fast8_t p = 0; p < 32; p += 8) if (((depth >> p) & 0xff) > max) max = (depth >> p) & 0xff;
  uint64_t * buffer;
  if (context -> source -> palette) {
    buffer = ctxmalloc(context, sizeof *buffer * (context -> source -> max_palette_index + 1));
    plum_convert_colors(buffer, context -> source -> palette, context -> source -> max_palette_index + 1, PLUM_COLOR_64 | PLUM_ALPHA_INVERT,
                        context -> source -> color_format);
  } else
    buffer = ctxmalloc(context, sizeof *buffer * context -> source -> width * context -> source -> height);
  if (depth < 0x1000000u)
    generate_PPM_data(context, NULL, max, buffer);
  else if (context -> source -> frames == 1)
    generate_PAM_data(context, max, buffer);
  else {
    uint32_t * sizes = get_true_PNM_frame_sizes(context);
    if (sizes) {
      generate_PPM_data(context, sizes, max, buffer);
      ctxfree(context, sizes);
    } else
      generate_PAM_data(context, max, buffer);
  }
  ctxfree(context, buffer);
}

uint32_t * get_true_PNM_frame_sizes (struct context * context) {
  // returns width, height pairs for each frame if the only transparency in those frames is an empty border on the bottom and right edges
  unsigned char format = context -> source -> color_format & PLUM_COLOR_MASK;
  uint64_t mask = alpha_component_masks[format], check = 0, color = get_background_color(context -> source, 0) & ~mask;
  if (context -> source -> color_format & PLUM_ALPHA_INVERT)
    check = mask;
  else
    color |= mask;
  uint32_t * result = ctxmalloc(context, sizeof *result * 2 * context -> source -> frames);
  size_t width, height, offset = (size_t) context -> source -> width * context -> source -> height;
  if (context -> source -> palette) {
    unsigned char colorclass[0x100]; // 0 for a solid color, 1 for empty pixels (fully transparent background), 2 for everything else
    #define checkclasses(bits) do                                                     \
      for (uint_fast16_t p = 0; p <= context -> source -> max_palette_index; p ++)    \
        if (context -> source -> palette ## bits[p] == color)                         \
          colorclass[p] = 1;                                                          \
        else if ((context -> source -> palette ## bits[p] & mask) == check)           \
          colorclass[p] = 0;                                                          \
        else                                                                          \
          colorclass[p] = 2;                                                          \
    while (0)
    if (format == PLUM_COLOR_16)
      checkclasses(16);
    else if (format == PLUM_COLOR_64)
      checkclasses(64);
    else
      checkclasses(32);
    #undef checkclasses
    for (uint_fast32_t frame = 0; frame < context -> source -> frames; frame ++) {
      const uint8_t * data = context -> source -> data8 + offset * frame;
      if (colorclass[*data]) goto fail;
      for (width = 1; width < context -> source -> width; width ++) if (colorclass[data[width]]) break;
      for (height = 1; height < context -> source -> height; height ++) if (colorclass[data[height * context -> source -> width]]) break;
      for (size_t row = 0; row < context -> source -> height; row ++) for (size_t col = 0; col < context -> source -> width; col ++)
        if (colorclass[data[row * context -> source -> width + col]] != (row >= height || col >= width)) goto fail;
      result[frame * 2] = width;
      result[frame * 2 + 1] = height;
    }
  } else {
    #define checkframe(bits) do                                                                                                             \
      for (uint_fast32_t frame = 0; frame < context -> source -> frames; frame ++) {                                                        \
        const uint ## bits ## _t * data = context -> source -> data ## bits + offset * frame;                                               \
        if (*data == color) goto fail;                                                                                                      \
        for (width = 1; width < context -> source -> width; width ++) if (data[width] == color) break;                                      \
        for (height = 1; height < context -> source -> height; height ++) if (data[height * context -> source -> width] == color) break;    \
        for (size_t row = 0; row < height; row ++) for (size_t col = 0; col < width; col ++)                                                \
          if ((data[row * context -> source -> width + col] & mask) != check) goto fail;                                                    \
        for (size_t row = 0; row < context -> source -> height; row ++)                                                                     \
          for (size_t col = (row < height) ? width : 0; col < context -> source -> width; col ++)                                           \
            if (data[row * context -> source -> width + col] != color) goto fail;                                                           \
        result[frame * 2] = width;                                                                                                          \
        result[frame * 2 + 1] = height;                                                                                                     \
      }                                                                                                                                     \
    while (0)
    if (format == PLUM_COLOR_16)
      checkframe(16);
    else if (format == PLUM_COLOR_64)
      checkframe(64);
    else
      checkframe(32);
    #undef checkframe
  }
  width = height = 0;
  for (uint_fast32_t frame = 0; frame < context -> source -> frames && !(width && height); frame ++) {
    if (result[frame * 2] == context -> source -> width) width = 1;
    if (result[frame * 2 + 1] == context -> source -> height) height = 1;
  }
  if (width && height) return result;
  fail:
  ctxfree(context, result);
  return NULL;
}

void generate_PPM_data (struct context * context, const uint32_t * sizes, unsigned bitdepth, uint64_t * restrict buffer) {
  size_t offset = (size_t) context -> source -> width * context -> source -> height;
  if (!context -> source -> palette) offset = plum_color_buffer_size(offset, context -> source -> color_format);
  for (size_t frame = 0; frame < context -> source -> frames; frame ++) {
    size_t width = sizes ? sizes[frame * 2] : context -> source -> width;
    size_t height = sizes ? sizes[frame * 2 + 1] : context -> source -> height;
    generate_PPM_header(context, width, height, bitdepth);
    if (context -> source -> palette)
      generate_PNM_frame_data_from_palette(context, context -> source -> data8 + offset * frame, buffer, width, height, bitdepth, 0);
    else {
      plum_convert_colors(buffer, context -> source -> data8 + offset * frame, height * context -> source -> width, PLUM_COLOR_64 | PLUM_ALPHA_INVERT,
                          context -> source -> color_format);
      generate_PNM_frame_data(context, buffer, width, height, bitdepth, 0);
    }
  }
}

void generate_PPM_header (struct context * context, uint32_t width, uint32_t height, unsigned bitdepth) {
  unsigned char * node = append_output_node(context, 32);
  size_t offset = byteappend(node, 0x50, 0x36, 0x0a); // P6<newline>
  offset += write_PNM_number(node + offset, width);
  node[offset ++] = 0x20; // space
  offset += write_PNM_number(node + offset, height);
  node[offset ++] = 0x0a; // newline
  offset += write_PNM_number(node + offset, ((uint32_t) 1 << bitdepth) - 1);
  node[offset ++] = 0x0a; // newline
  context -> output -> size = offset;
}

void generate_PAM_data (struct context * context, unsigned bitdepth, uint64_t * restrict buffer) {
  size_t size = (size_t) context -> source -> width * context -> source -> height, offset = plum_color_buffer_size(size, context -> source -> color_format);
  for (uint_fast32_t frame = 0; frame < context -> source -> frames; frame ++) {
    generate_PAM_header(context, bitdepth);
    if (context -> source -> palette)
      generate_PNM_frame_data_from_palette(context, context -> source -> data8 + size * frame, buffer, context -> source -> width, context -> source -> height,
                                           bitdepth, 1);
    else {
      plum_convert_colors(buffer, context -> source -> data8 + offset * frame, size, PLUM_COLOR_64 | PLUM_ALPHA_INVERT, context -> source -> color_format);
      generate_PNM_frame_data(context, buffer, context -> source -> width, context -> source -> height, bitdepth, 1);
    }
  }
}

void generate_PAM_header (struct context * context, unsigned bitdepth) {
  unsigned char * node = append_output_node(context, 96);
  size_t offset = byteappend(node,
                             0x50, 0x37, 0x0a, // P7<newline>
                             0x54, 0x55, 0x50, 0x4c, 0x54, 0x59, 0x50, 0x45, 0x20, // TUPLTYPE<space>
                             0x52, 0x47, 0x42, 0x5f, 0x41, 0x4c, 0x50, 0x48, 0x41, 0x0a, // RGB_ALPHA<newline>
                             0x57, 0x49, 0x44, 0x54, 0x48, 0x20 // WIDTH<space>
                            );
  offset += write_PNM_number(node + offset, context -> source -> width);
  offset += byteappend(node + offset, 0x0a, 0x48, 0x45, 0x49, 0x47, 0x48, 0x54, 0x20); // <newline>HEIGHT<space>
  offset += write_PNM_number(node + offset, context -> source -> height);
  offset += byteappend(node + offset, 0x0a, 0x4d, 0x41, 0x58, 0x56, 0x41, 0x4c, 0x20); // <newline>MAXVAL<space>
  offset += write_PNM_number(node + offset, ((uint32_t) 1 << bitdepth) - 1);
  offset += byteappend(node + offset,
                       0x0a, // <newline>
                       0x44, 0x45, 0x50, 0x54, 0x48, 0x20, 0x34, 0x0a, // DEPTH 4<newline>
                       0x45, 0x4e, 0x44, 0x48, 0x44, 0x52, 0x0a // ENDHDR<newline>
                      );
  context -> output -> size = offset;
}

size_t write_PNM_number (unsigned char * buffer, uint32_t number) {
  // won't work for 0, but there's no need to write a 0 anywhere
  unsigned char data[10];
  uint_fast8_t size = 0;
  while (number) {
    data[size ++] = 0x30 + number % 10;
    number /= 10;
  }
  for (uint_fast8_t p = size; p; *(buffer ++) = data[-- p]);
  return size;
}

void generate_PNM_frame_data (struct context * context, const uint64_t * data, uint32_t width, uint32_t height, unsigned bitdepth, int alpha) {
  uint_fast8_t shift = 16 - bitdepth, mask = (1 << ((bitdepth > 8) ? bitdepth - 8 : bitdepth)) - 1;
  const uint64_t * rowdata = data;
  unsigned char * output = append_output_node(context, (size_t) (3 + !!alpha) * ((bitdepth + 7) / 8) * width * height);
  if (shift >= 8)
    for (uint_fast32_t row = 0; row < height; row ++, rowdata += context -> source -> width) for (uint_fast32_t col = 0; col < width; col ++) {
      output += byteappend(output, (rowdata[col] >> shift) & mask, (rowdata[col] >> (shift + 16)) & mask, (rowdata[col] >> (shift + 32)) & mask);
      if (alpha) *(output ++) = rowdata[col] >> (shift + 48);
    }
  else
    for (uint_fast32_t row = 0; row < height; row ++, rowdata += context -> source -> width) for (uint_fast32_t col = 0; col < width; col ++) {
      output += byteappend(output, (rowdata[col] >> (shift + 8)) & mask, rowdata[col] >> shift, (rowdata[col] >> (shift + 24)) & mask,
                                   rowdata[col] >> (shift + 16), (rowdata[col] >> (shift + 40)) & mask, rowdata[col] >> (shift + 32));
      if (alpha) output += byteappend(output, rowdata[col] >> (shift + 56), rowdata[col] >> (shift + 48));
    }
}

void generate_PNM_frame_data_from_palette (struct context * context, const uint8_t * data, const uint64_t * palette, uint32_t width, uint32_t height,
                                           unsigned bitdepth, int alpha) {
  // very similar to the previous function, but adjusted to use the color from the palette and to read 8-bit data
  uint_fast8_t shift = 16 - bitdepth, mask = (1 << ((bitdepth > 8) ? bitdepth - 8 : bitdepth)) - 1;
  const uint8_t * rowdata = data;
  unsigned char * output = append_output_node(context, (size_t) (3 + !!alpha) * ((bitdepth + 7) / 8) * width * height);
  if (shift >= 8)
    for (uint_fast32_t row = 0; row < height; row ++, rowdata += context -> source -> width) for (uint_fast32_t col = 0; col < width; col ++) {
      uint64_t color = palette[rowdata[col]];
      output += byteappend(output, (color >> shift) & mask, (color >> (shift + 16)) & mask, (color >> (shift + 32)) & mask);
      if (alpha) *(output ++) = color >> (shift + 48);
    }
  else
    for (uint_fast32_t row = 0; row < height; row ++, rowdata += context -> source -> width) for (uint_fast32_t col = 0; col < width; col ++) {
      uint64_t color = palette[rowdata[col]];
      output += byteappend(output, (color >> (shift + 8)) & mask, color >> shift, (color >> (shift + 24)) & mask, color >> (shift + 16),
                                   (color >> (shift + 40)) & mask, color >> (shift + 32));
      if (alpha) output += byteappend(output, color >> (shift + 56), color >> (shift + 48));
    }
}
