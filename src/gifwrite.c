#include "proto.h"

void generate_GIF_data (struct context * context) {
  if (context -> source -> width > 0xffffu || context -> source -> height > 0xffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  // technically, some GIFs could be 87a; however, at the time of writing, 89a is over three decades old and supported by everything relevant
  byteoutput(context, 0x47, 0x49, 0x46, 0x38, 0x39, 0x61);
  unsigned char * header = append_output_node(context, 7);
  write_le16_unaligned(header, context -> source -> width);
  write_le16_unaligned(header + 2, context -> source -> height);
  uint_fast32_t depth = get_true_color_depth(context -> source);
  uint8_t overall = depth;
  if ((uint8_t) (depth >> 8) > overall) overall = depth >> 8;
  if ((uint8_t) (depth >> 16) > overall) overall = depth >> 16;
  if (overall > 8) overall = 8;
  bytewrite(header + 4, (overall - 1) << 4, 0, 0);
  if (context -> source -> palette)
    generate_GIF_data_with_palette(context, header);
  else
    generate_GIF_data_from_raw(context, header);
  byteoutput(context, 0x3b);
}

#define checkboundaries(buffertype, buffer, frame) do {                                                                          \
  size_t index = 0;                                                                                                              \
  if (boundaries) {                                                                                                              \
    while (index < context -> source -> width * boundaries[frame].top) if (buffer[index ++] != transparent) goto findbounds;     \
    for (uint_fast16_t row = 0; row < boundaries[frame].height; row ++) {                                                        \
      for (uint_fast16_t col = 0; col < boundaries[frame].left; col ++) if (buffer[index ++] != transparent) goto findbounds;    \
      index += boundaries[frame].width;                                                                                          \
      for (uint_fast16_t col = boundaries[frame].left + boundaries[frame].width; col < context -> source -> width; col ++)       \
        if (buffer[index ++] != transparent) goto findbounds;                                                                    \
    }                                                                                                                            \
    while (index < framesize) if (buffer[index ++] != transparent) goto findbounds;                                              \
    left = boundaries[frame].left;                                                                                               \
    top = boundaries[frame].top;                                                                                                 \
    width = boundaries[frame].width;                                                                                             \
    height = boundaries[frame].height;                                                                                           \
  } else {                                                                                                                       \
    findbounds:                                                                                                                  \
    for (index = 0; index < framesize; index ++) if (buffer[index] != transparent) break;                                        \
    if (index == framesize)                                                                                                      \
      width = height = 1;                                                                                                        \
    else {                                                                                                                       \
      top = index / width;                                                                                                       \
      height -= top;                                                                                                             \
      for (index = 0; index < framesize; index ++) if (buffer[framesize - 1 - index] != transparent) break;                      \
      height -= index / width;                                                                                                   \
      for (left = 0; left < width; left ++) for (index = top; index < top + height; index ++)                                    \
        if (buffer[index * context -> source -> width + left] != transparent) goto leftdone;                                     \
      leftdone:                                                                                                                  \
      width -= left;                                                                                                             \
      uint_fast16_t col;                                                                                                         \
      for (col = 0; col < width; col ++) for (index = top; index < top + height; index ++)                                       \
        if (buffer[(index + 1) * context -> source -> width - 1 - col] != transparent) goto rightdone;                           \
      rightdone:                                                                                                                 \
      width -= col;                                                                                                              \
    }                                                                                                                            \
  }                                                                                                                              \
  if (left || width != context -> source -> width) {                                                                             \
    buffertype * target = buffer;                                                                                                \
    for (uint_fast16_t row = 0; row < height; row ++) for (uint_fast16_t col = 0; col < width; col ++)                           \
      *(target ++) = buffer[context -> source -> width * (row + top) + col + left];                                              \
  } else if (top)                                                                                                                \
    memmove(buffer, buffer + context -> source -> width * top, sizeof *buffer * context -> source -> width * height);            \
} while (false)

void generate_GIF_data_with_palette (struct context * context, unsigned char * header) {
  uint_fast16_t colors = context -> source -> max_palette_index + 1;
  uint32_t * palette = ctxcalloc(context, 256 * sizeof *palette);
  plum_convert_colors(palette, context -> source -> palette, colors, PLUM_COLOR_32, context -> source -> color_format);
  int transparent = -1;
  uint8_t * mapping = NULL;
  for (uint_fast16_t p = 0; p <= context -> source -> max_palette_index; p ++) {
    if (palette[p] & 0x80000000u)
      if (transparent < 0)
        transparent = p;
      else {
        if (!mapping) {
          mapping = ctxmalloc(context, colors * sizeof *mapping);
          for (uint_fast16_t index = 0; index <= context -> source -> max_palette_index; index ++) mapping[index] = index;
        }
        mapping[p] = transparent;
      }
    palette[p] &= 0xffffffu;
  }
  int_fast32_t background = get_GIF_background_color(context);
  if (background >= 0) {
    uint_fast16_t index;
    for (index = 0; index < colors; index ++) if (palette[index] == background) break;
    if (index == colors && colors < 256) palette[colors ++] = background;
    header[5] = index; // if index > 255, this truncates, but it doesn't matter because any value would be wrong in that case
  }
  uint_fast16_t colorbits;
  for (colorbits = 0; colors > (2 << colorbits); colorbits ++);
  header[4] |= 0x80 + colorbits;
  uint_fast16_t colorcount = 2 << colorbits;
  write_GIF_palette(context, palette, colorcount);
  ctxfree(context, palette);
  write_GIF_loop_info(context);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  unsigned char * framebuffer = ctxmalloc(context, framesize);
  const struct plum_metadata * durations = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DURATION);
  const struct plum_metadata * disposals = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DISPOSAL);
  int64_t duration_remainder = 0;
  struct plum_rectangle * boundaries = get_frame_boundaries(context, false);
  for (uint_fast32_t frame = 0; frame < context -> source -> frames; frame ++) {
    if (mapping)
      for (size_t pixel = 0; pixel < framesize; pixel ++) framebuffer[pixel] = mapping[context -> source -> data8[frame * framesize + pixel]];
    else
      memcpy(framebuffer, context -> source -> data8 + frame * framesize, framesize);
    uint_fast16_t left = 0, top = 0, width = context -> source -> width, height = context -> source -> height;
    if (transparent >= 0) checkboundaries(unsigned char, framebuffer, frame);
    write_GIF_frame(context, framebuffer, NULL, colorcount, transparent, frame, left, top, width, height, durations, disposals, &duration_remainder);
  }
  ctxfree(context, boundaries);
  ctxfree(context, mapping);
  ctxfree(context, framebuffer);
}

void generate_GIF_data_from_raw (struct context * context, unsigned char * header) {
  int_fast32_t background = get_GIF_background_color(context);
  if (background >= 0) {
    header[4] |= 0x80;
    write_GIF_palette(context, (const uint32_t []) {background, 0}, 2);
  }
  write_GIF_loop_info(context);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  uint32_t * colorbuffer = ctxmalloc(context, sizeof *colorbuffer * framesize);
  unsigned char * framebuffer = ctxmalloc(context, framesize);
  const struct plum_metadata * durations = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DURATION);
  const struct plum_metadata * disposals = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DISPOSAL);
  size_t offset = plum_color_buffer_size(framesize, context -> source -> color_format);
  int64_t duration_remainder = 0;
  struct plum_rectangle * boundaries = get_frame_boundaries(context, false);
  for (uint_fast32_t frame = 0; frame < context -> source -> frames; frame ++) {
    plum_convert_colors(colorbuffer, context -> source -> data8 + offset * frame, framesize, PLUM_COLOR_32, context -> source -> color_format);
    generate_GIF_frame_data(context, colorbuffer, framebuffer, frame, durations, disposals, &duration_remainder, boundaries ? boundaries + frame : NULL);
  }
  ctxfree(context, boundaries);
  ctxfree(context, framebuffer);
  ctxfree(context, colorbuffer);
}

void generate_GIF_frame_data (struct context * context, uint32_t * restrict pixels, unsigned char * restrict framebuffer, uint32_t frame,
                              const struct plum_metadata * durations, const struct plum_metadata * disposals, int64_t * restrict duration_remainder,
                              const struct plum_rectangle * boundaries) {
  size_t framesize = (size_t) context -> source -> height * context -> source -> width;
  uint32_t transparent = 0;
  for (size_t index = 0; index < framesize; index ++)
    if (pixels[index] & 0x80000000u) {
      if (!transparent) transparent = 0xff000000u | pixels[index];
      pixels[index] = transparent;
    } else
      pixels[index] &= 0xffffffu;
  uint_fast16_t left = 0, top = 0, width = context -> source -> width, height = context -> source -> height;
  if (transparent) checkboundaries(uint32_t, pixels, 0);
  uint32_t * palette = ctxcalloc(context, 256 * sizeof *palette);
  int colorcount = plum_convert_colors_to_indexes(framebuffer, pixels, palette, (size_t) width * height, PLUM_COLOR_32);
  if (colorcount < 0) throw(context, -colorcount);
  int transparent_index = -1;
  if (transparent)
    for (uint_fast16_t index = 0; index <= colorcount; index ++) if (palette[index] == transparent) {
      transparent_index = index;
      break;
    }
  write_GIF_frame(context, framebuffer, palette, colorcount + 1, transparent_index, frame, left, top, width, height, durations, disposals, duration_remainder);
  ctxfree(context, palette);
}

#undef checkboundaries

int_fast32_t get_GIF_background_color (struct context * context) {
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_BACKGROUND);
  if (!metadata) return -1;
  uint32_t converted;
  plum_convert_colors(&converted, metadata -> data, 1, PLUM_COLOR_32, context -> source -> color_format);
  return converted & 0xffffffu;
}

void write_GIF_palette (struct context * context, const uint32_t * restrict palette, unsigned count) {
  for (unsigned char * data = append_output_node(context, 3 * count); count; count --, palette ++)
    data += byteappend(data, *palette, *palette >> 8, *palette >> 16);
}

void write_GIF_loop_info (struct context * context) {
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_LOOP_COUNT);
  if (!metadata) return;
  uint_fast32_t count = *(const uint32_t *) metadata -> data;
  if (count > 0xffffu) count = 0; // too many loops, so just make it loop forever
  if (count == 1) return;
  byteoutput(context, 0x21, 0xff, 0x0b, 0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, count, count >> 8, 0x00);
}

void write_GIF_frame (struct context * context, const unsigned char * restrict data, const uint32_t * restrict palette, unsigned colors, int transparent,
                      uint32_t frame, unsigned left, unsigned top, unsigned width, unsigned height, const struct plum_metadata * durations,
                      const struct plum_metadata * disposals, int64_t * restrict duration_remainder) {
  uint64_t duration = 0;
  uint8_t disposal = 0;
  if (durations && durations -> size > sizeof(uint64_t) * frame) {
    duration = frame[(const uint64_t *) durations -> data];
    if (duration) {
      if (duration == 1) duration = 0;
      uint64_t true_duration = adjust_frame_duration(duration, duration_remainder);
      duration = (true_duration / 5000000u + 1) >> 1;
      if (duration > 0xffffu) duration = 0xffffu; // maxed out
      update_frame_duration_remainder(true_duration, duration * 10000000u, duration_remainder);
    }
  }
  if (disposals && disposals -> size > frame) {
    disposal = frame[(const uint8_t *) disposals -> data];
    if (disposal >= PLUM_DISPOSAL_REPLACE) disposal -= PLUM_DISPOSAL_REPLACE;
  }
  uint_fast8_t colorbits;
  for (colorbits = 0; colors > (2 << colorbits); colorbits ++);
  unsigned colorcount = 2 << colorbits;
  byteoutput(context, 0x21, 0xf9, 0x04, (disposal + 1) * 4 + (transparent >= 0), duration, duration >> 8, (transparent >= 0) ? transparent : 0, 0x00,
                      0x2c, left, left >> 8, top, top >> 8, width, width >> 8, height, height >> 8, colorbits | (palette ? 0x80 : 0));
  if (palette) write_GIF_palette(context, palette, colorcount);
  if (!colorbits) colorbits = 1;
  byteoutput(context, ++ colorbits); // incremented because compression starts with one bit extra
  size_t length;
  unsigned char * output = compress_GIF_data(context, data, (size_t) width * height, &length, colorbits);
  write_GIF_data_blocks(context, output, length);
  ctxfree(context, output);
}

void write_GIF_data_blocks (struct context * context, const unsigned char * restrict data, size_t size) {
  uint_fast8_t remainder = size % 0xff;
  size /= 0xff;
  unsigned char * output = append_output_node(context, size * 0x100 + (remainder ? remainder + 2 : 1));
  while (size --) {
    *(output ++) = 0xff;
    memcpy(output, data, 0xff);
    output += 0xff;
    data += 0xff;
  }
  if (remainder) {
    *(output ++) = remainder;
    memcpy(output, data, remainder);
    output += remainder;
  }
  *output = 0;
}
