#include "proto.h"

void generate_GIF_data (struct context * context) {
  if ((context -> source -> width > 0xffffu) || (context -> source -> height > 0xffffu)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  // technically, some GIFs could be 87a; however, at the time of writing, 89a is over three decades old and supported by everything relevant
  byteoutput(context, 0x47, 0x49, 0x46, 0x38, 0x39, 0x61);
  unsigned char * header = append_output_node(context, 7);
  write_le16((uint16_t *) header, context -> source -> width);
  write_le16((uint16_t *) (header + 2), context -> source -> height);
  uint_fast32_t depth = get_true_color_depth(context -> source);
  uint8_t overall = depth;
  if ((uint8_t) (depth >> 8) > overall) overall = depth >> 8;
  if ((uint8_t) (depth >> 16) > overall) overall = depth >> 16;
  if (overall > 8) overall = 8;
  header[4] = (overall - 1) << 4;
  header[5] = header[6] = 0;
  if (context -> source -> palette)
    generate_GIF_data_with_palette(context, header);
  else
    generate_GIF_data_from_raw(context, header);
  byteoutput(context, 0x3b);
}

void generate_GIF_data_with_palette (struct context * context, unsigned char * header) {
  uint_fast16_t colors = context -> source -> max_palette_index + 1;
  uint32_t * palette = ctxcalloc(context, 256 * sizeof *palette);
  plum_convert_colors(palette, context -> source -> palette, context -> source -> max_palette_index + 1, PLUM_COLOR_32, context -> source -> color_format);
  int transparent = -1;
  uint8_t * mapping = NULL;
  uint_fast32_t p;
  for (p = 0; p <= context -> source -> max_palette_index; p ++) if (palette[p] & 0x80000000u)
    if (transparent < 0)
      transparent = p;
    else {
      if (!mapping) {
        mapping = ctxmalloc(context, colors * sizeof *mapping);
        unsigned index;
        for (index = 0; index <= context -> source -> max_palette_index; index ++) mapping[index] = index;
      }
      mapping[p] = transparent;
    }
  for (p = 0; p <= context -> source -> max_palette_index; p ++) palette[p] &= 0xffffffu;
  int_fast32_t background = get_GIF_background_color(context);
  if (background >= 0) {
    for (p = 0; p < colors; p ++) if (palette[p] == background) break;
    if (p >= colors) {
      if (colors == 256) throw(context, PLUM_ERR_TOO_MANY_COLORS);
      palette[colors ++] = background;
    }
    background = p;
  }
  for (p = 0; colors > (2 << p); p ++);
  colors = 2 << p;
  header[4] |= 0x80 + p;
  if (background >= 0) header[5] = background;
  write_GIF_palette(context, palette, colors);
  ctxfree(context, palette);
  write_GIF_loop_info(context);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  unsigned char * framebuffer = ctxmalloc(context, framesize);
  const struct plum_metadata * durations = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DURATION);
  const struct plum_metadata * disposals = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DISPOSAL);
  for (p = 0; p < context -> source -> frames; p ++) {
    if (mapping) {
      size_t pixel;
      for (pixel = 0; pixel < framesize; pixel ++) framebuffer[pixel] = mapping[context -> source -> data8[p * framesize + pixel]];
    } else
      memcpy(framebuffer, context -> source -> data8 + p * framesize, framesize);
    uint_fast16_t left = 0, top = 0, width = context -> source -> width, height = context -> source -> height;
    if (transparent >= 0) {
      size_t index;
      for (index = 0; index < framesize; index ++) if (framebuffer[index] != transparent) break;
      if (index == framesize)
        width = height = 1;
      else {
        top = index / width;
        height -= top;
        for (index = 0; index < framesize; index ++) if (framebuffer[framesize - 1 - index] != transparent) break;
        height -= index / width;
        for (left = 0; left < width; left ++) for (index = top; index < (top + height); index ++)
          if (framebuffer[index * context -> source -> width + left] != transparent) goto leftdone;
        leftdone:
        width -= left;
        uint_fast16_t col;
        for (col = 0; col < width; col ++) for (index = top; index < (top + height); index ++)
          if (framebuffer[(index + 1) * context -> source -> width - 1 - col] != transparent) goto rightdone;
        rightdone:
        width -= col;
        if (left || (width != context -> source -> width)) {
          unsigned char * target = framebuffer;
          for (index = 0; index < height; index ++) for (col = 0; col < width; col ++)
            *(target ++) = framebuffer[(index + top) * context -> source -> width + col + left];
        } else if (top)
          memmove(framebuffer, framebuffer + context -> source -> width * top, context -> source -> width * height);
      }
    }
    write_GIF_frame(context, framebuffer, NULL, colors, transparent, p, left, top, width, height, durations, disposals);
  }
  if (mapping) ctxfree(context, mapping);
  ctxfree(context, framebuffer);
}

void generate_GIF_data_from_raw (struct context * context, unsigned char * header) {
  int_fast32_t background = get_GIF_background_color(context);
  if (background >= 0) {
    header[4] |= 0x80;
    uint32_t palette[2] = {background, 0};
    write_GIF_palette(context, palette, 2);
  }
  write_GIF_loop_info(context);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  uint32_t * colorbuffer = ctxmalloc(context, sizeof *colorbuffer * framesize);
  unsigned char * framebuffer = ctxmalloc(context, framesize);
  uint_fast32_t frame;
  const struct plum_metadata * durations = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DURATION);
  const struct plum_metadata * disposals = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DISPOSAL);
  for (frame = 0; frame < context -> source -> frames; frame ++) {
    if ((context -> source -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_64)
      plum_convert_colors(colorbuffer, context -> source -> data64 + framesize * frame, framesize, PLUM_COLOR_32, context -> source -> color_format);
    else if ((context -> source -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_16)
      plum_convert_colors(colorbuffer, context -> source -> data16 + framesize * frame, framesize, PLUM_COLOR_32, context -> source -> color_format);
    else
      plum_convert_colors(colorbuffer, context -> source -> data32 + framesize * frame, framesize, PLUM_COLOR_32, context -> source -> color_format);
    generate_GIF_frame_data(context, colorbuffer, framebuffer, frame, durations, disposals);
  }
  ctxfree(context, framebuffer);
  ctxfree(context, colorbuffer);
}

void generate_GIF_frame_data (struct context * context, uint32_t * restrict pixels, unsigned char * restrict framebuffer, uint32_t frame,
                              const struct plum_metadata * durations, const struct plum_metadata * disposals) {
  size_t index, framesize = (size_t) context -> source -> height * context -> source -> width;
  uint32_t transparent = 0;
  for (index = 0; index < framesize; index ++)
    if (pixels[index] & 0x80000000u) {
      if (!transparent) transparent = 0xff000000u | pixels[index];
      pixels[index] = transparent;
    } else
      pixels[index] &= 0xffffffu;
  uint_fast16_t left = 0, top = 0, width = context -> source -> width, height = context -> source -> height;
  if (transparent) {
    for (index = 0; index < framesize; index ++) if (pixels[index] != transparent) break;
    if (index == framesize)
      width = height = 1;
    else {
      top = index / width;
      height -= top;
      for (index = 0; index < framesize; index ++) if (pixels[framesize - 1 - index] != transparent) break;
      height -= index / width;
      for (left = 0; left < width; left ++) for (index = top; index < (top + height); index ++)
        if (pixels[index * context -> source -> width + left] != transparent) goto leftdone;
      leftdone:
      width -= left;
      uint_fast16_t col;
      for (col = 0; col < width; col ++) for (index = top; index < (top + height); index ++)
        if (pixels[(index + 1) * context -> source -> width - 1 - col] != transparent) goto rightdone;
      rightdone:
      width -= col;
      if (left || (width != context -> source -> width)) {
        uint32_t * target = pixels;
        for (index = 0; index < height; index ++) for (col = 0; col < width; col ++)
          *(target ++) = pixels[(index + top) * context -> source -> width + col + left];
      } else if (top)
        memmove(pixels, pixels + context -> source -> width * top, sizeof *pixels * context -> source -> width * height);
    }
  }
  uint32_t * palette = ctxcalloc(context, 256 * sizeof *palette);
  int colorcount = plum_convert_colors_to_indexes(framebuffer, pixels, palette, (size_t) width * height, PLUM_COLOR_32);
  if (colorcount < 0) throw(context, -colorcount);
  int transparent_index = -1;
  if (transparent)
    for (index = 0; index <= colorcount; index ++) if (palette[index] == transparent) {
      transparent_index = transparent;
      break;
    }
  write_GIF_frame(context, framebuffer, palette, colorcount + 1, transparent_index, frame, left, top, width, height, durations, disposals);
  ctxfree(context, palette);
}

int_fast32_t get_GIF_background_color (struct context * context) {
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_BACKGROUND);
  if (!metadata) return -1;
  uint32_t converted;
  plum_convert_colors(&converted, metadata -> data, 1, PLUM_COLOR_32, context -> source -> color_format);
  return converted & 0xffffffu;
}

void write_GIF_palette (struct context * context, const uint32_t * palette, unsigned count) {
  unsigned char * data = append_output_node(context, 3 * count);
  while (count --) {
    *(data ++) = *palette;
    *(data ++) = *palette >> 8;
    *(data ++) = *(palette ++) >> 16;
  }
}

void write_GIF_loop_info (struct context * context) {
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_LOOP_COUNT);
  if (!metadata) return;
  const uint32_t * count = metadata -> data;
  if (*count > 0xffff) count = 0; // too many loops, so just make it loop forever
  if (*count == 1) return;
  unsigned char data[] = {0x21, 0xff, 0x0b, 0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, *count, *count >> 8, 0x00};
  char * output = append_output_node(context, sizeof data);
  memcpy(output, data, sizeof data);
}

void write_GIF_frame (struct context * context, const unsigned char * restrict data, const uint32_t * palette, unsigned colors, int transparent,
                      uint32_t frame, unsigned left, unsigned top, unsigned width, unsigned height, const struct plum_metadata * durations,
                      const struct plum_metadata * disposals) {
  uint64_t duration = 0;
  uint8_t disposal = 0;
  if (durations && (durations -> size > (sizeof(uint64_t) * frame))) {
    duration = frame[(const uint64_t *) durations -> data];
    duration = (duration + 5000000u) / 10000000u;
    if (duration > 0xffffu) duration = 0xffffu; // maxed out
  }
  if (disposals && (disposals -> size > frame)) {
    disposal = frame[(const uint8_t *) disposals -> data];
    if (disposal >= PLUM_DISPOSAL_REPLACE) disposal -= PLUM_DISPOSAL_REPLACE;
  }
  unsigned char extraflags = (disposal + 1) << 2;
  if (transparent >= 0) extraflags ++;
  unsigned char baseflags;
  for (baseflags = 0; colors > (2 << baseflags); baseflags ++);
  colors = 2 << baseflags;
  if (palette) baseflags |= 0x80;
  unsigned char header[] = {0x21, 0xf9, 0x04, extraflags, duration, duration >> 8, (transparent >= 0) ? transparent : 0, 0x00,
                            0x2c, left, left >> 8, top, top >> 8, width, width >> 8, height, height >> 8, baseflags};
  memcpy(append_output_node(context, sizeof header), header, sizeof header);
  if (palette) write_GIF_palette(context, palette, colors);
  unsigned codesize = (baseflags & 7) + 1;
  if (codesize < 2) codesize = 2;
  byteoutput(context, codesize);
  size_t length;
  unsigned char * output = compress_GIF_data(context, data, (size_t) width * height, &length, codesize);
  write_GIF_data_blocks(context, output, length);
  ctxfree(context, output);
}

void write_GIF_data_blocks (struct context * context, const unsigned char * restrict data, size_t size) {
  uint8_t remainder = size % 0xff;
  size /= 0xff;
  unsigned char * output = append_output_node(context, size * 0x100 + remainder + !!remainder + 1);
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
