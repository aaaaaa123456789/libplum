#include "proto.h"

void generate_PNG_data (struct context * context) {
  if (context -> source -> frames > 1) throw(context, PLUM_ERR_NO_MULTI_FRAME);
  unsigned type = generate_PNG_header(context, NULL);
  append_PNG_image_data(context, context -> source -> data, type, NULL, NULL);
  output_PNG_chunk(context, 0x49454e44u, 0, NULL); // IEND
}

void generate_APNG_data (struct context * context) {
  if (context -> source -> frames > 0x40000000u) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  struct plum_rectangle * boundaries = get_frame_boundaries(context, false);
  unsigned type = generate_PNG_header(context, boundaries);
  uint32_t loops = 1;
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_LOOP_COUNT);
  if (metadata) {
    loops = *(uint32_t *) metadata -> data;
    if (loops > 0x7fffffffu) loops = 0; // too many loops, so just make it loop forever
  }
  const uint64_t * durations = NULL;
  const uint8_t * disposals = NULL;
  size_t duration_count = 0, disposal_count = 0;
  if (metadata = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DURATION)) {
    durations = metadata -> data;
    duration_count = metadata -> size / sizeof(uint64_t);
  }
  if (metadata = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_DISPOSAL)) {
    disposals = metadata -> data;
    disposal_count = metadata -> size;
  }
  uint32_t chunkID = 0;
  unsigned char animation_data[8];
  write_be32_unaligned(animation_data + 4, loops);
  int64_t duration_remainder = 0;
  if ((duration_count && *durations) || context -> source -> frames == 1) {
    write_be32_unaligned(animation_data, context -> source -> frames);
    output_PNG_chunk(context, 0x6163544cu, sizeof animation_data, animation_data); // acTL
    append_APNG_frame_header(context, duration_count ? *durations : 0, disposal_count ? *disposals : 0, &chunkID, &duration_remainder, NULL);
  } else {
    write_be32_unaligned(animation_data, context -> source -> frames - 1);
    output_PNG_chunk(context, 0x6163544cu, sizeof animation_data, animation_data); // acTL
  }
  append_PNG_image_data(context, context -> source -> data, type, NULL, NULL);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  if (!context -> source -> palette) framesize = plum_color_buffer_size(framesize, context -> source -> color_format);
  for (uint_fast32_t frame = 1; frame < context -> source -> frames; frame ++) {
    const struct plum_rectangle * rectangle = boundaries ? boundaries + frame : NULL;
    append_APNG_frame_header(context, (duration_count > frame) ? durations[frame] : 0, (disposal_count > frame) ? disposals[frame] : 0, &chunkID,
                             &duration_remainder, rectangle);
    append_PNG_image_data(context, context -> source -> data8 + framesize * frame, type, &chunkID, rectangle);
  }
  ctxfree(context, boundaries);
  output_PNG_chunk(context, 0x49454e44u, 0, NULL); // IEND
}

unsigned generate_PNG_header (struct context * context, struct plum_rectangle * restrict boundaries) {
  // returns the selected type of image: 0, 1, 2, 3: paletted (1 << type bits), 4, 5: 8-bit RGB (without and with alpha), 6, 7: 16-bit RGB
  // also updates the frame boundaries for APNG images (ensuring that frame 0 and frames with nonempty pixels outside their boundaries become full size)
  byteoutput(context, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a);
  bool transparency;
  if (boundaries) {
    *boundaries = (struct plum_rectangle) {.top = 0, .left = 0, .width = context -> source -> width, .height = context -> source -> height};
    adjust_frame_boundaries(context -> source, boundaries);
    transparency = image_rectangles_have_transparency(context -> source, boundaries);
  } else
    transparency = image_has_transparency(context -> source);
  uint32_t depth = get_color_depth(context -> source);
  if (!transparency) depth &= 0xffffffu;
  uint_fast8_t type;
  if (context -> source -> palette)
    if (context -> source -> max_palette_index < 2)
      type = 0;
    else if (context -> source -> max_palette_index < 4)
      type = 1;
    else if (context -> source -> max_palette_index < 16)
      type = 2;
    else
      type = 3;
  else if (bit_depth_less_than(depth, 0x8080808u))
    type = 4 + transparency;
  else
    type = 6 + transparency;
  append_PNG_header_chunks(context, type, depth);
  if (type < 4) append_PNG_palette_data(context, transparency);
  const struct plum_metadata * background = plum_find_metadata(context -> source, PLUM_METADATA_BACKGROUND);
  if (background) append_PNG_background_chunk(context, background -> data, type);
  return type;
}

void append_PNG_header_chunks (struct context * context, unsigned type, uint32_t depth) {
  if (context -> source -> width > 0x7fffffffu || context -> source -> height > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  unsigned char header[13];
  write_be32_unaligned(header, context -> image -> width);
  write_be32_unaligned(header + 4, context -> image -> height);
  bytewrite(header + 8, (type < 4) ? 1 << type : (8 << (type >= 6)), (type >= 4) ? 2 + 4 * (type & 1) : 3, 0, 0, 0);
  output_PNG_chunk(context, 0x49484452u, sizeof header, header); // IHDR
  unsigned char depthdata[4];
  write_le32_unaligned(depthdata, depth); // this will write each byte of depth in the expected position
  if (type < 4) {
    if (*depthdata > 8) *depthdata = 8;
    if (depthdata[1] > 8) depthdata[1] = 8;
    if (depthdata[2] > 8) depthdata[2] = 8;
  }
  output_PNG_chunk(context, 0x73424954u, 3 + ((type & 5) == 5), depthdata); // sBIT
}

void append_PNG_palette_data (struct context * context, bool use_alpha) {
  uint32_t color_buffer[256];
  plum_convert_colors(color_buffer, context -> source -> palette, context -> source -> max_palette_index + 1, PLUM_COLOR_32 | PLUM_ALPHA_INVERT,
                      context -> source -> color_format);
  unsigned char data[768];
  for (uint_fast16_t index = 0; index <= context -> source -> max_palette_index; index ++)
    bytewrite(data + index * 3, color_buffer[index], color_buffer[index] >> 8, color_buffer[index] >> 16);
  output_PNG_chunk(context, 0x504c5445u, 3 * (context -> source -> max_palette_index + 1), data); // PLTE
  if (use_alpha) {
    unsigned char alpha[256];
    for (uint_fast16_t index = 0; index <= context -> source -> max_palette_index; index ++) alpha[index] = color_buffer[index] >> 24;
    output_PNG_chunk(context, 0x74524e53u, context -> source -> max_palette_index + 1, alpha); // tRNS
  }
}

void append_PNG_background_chunk (struct context * context, const void * restrict data, unsigned type) {
  if (type >= 4) {
    unsigned char chunkdata[6];
    uint64_t color;
    plum_convert_colors(&color, data, 1, PLUM_COLOR_64, context -> source -> color_format);
    if (type < 6) color = (color >> 8) & 0xff00ff00ffu;
    write_be16_unaligned(chunkdata, color);
    write_be16_unaligned(chunkdata + 2, color >> 16);
    write_be16_unaligned(chunkdata + 4, color >> 32);
    output_PNG_chunk(context, 0x624b4744u, sizeof chunkdata, chunkdata); // bKGD
  } else {
    size_t size = plum_color_buffer_size(1, context -> source -> color_format);
    const unsigned char * current = context -> source -> palette;
    for (uint_fast16_t pos = 0; pos <= context -> source -> max_palette_index; pos ++, current += size) if (!memcmp(current, data, size)) {
      unsigned char byte = pos;
      output_PNG_chunk(context, 0x624b4744u, 1, &byte); // bKGD
      return;
    }
  }
}

void append_PNG_image_data (struct context * context, const void * restrict data, unsigned type, uint32_t * restrict chunkID,
                            const struct plum_rectangle * boundaries) {
  // chunkID counts animation data chunks (fcTL, fdAT); if chunkID is null, emit IDAT chunks instead
  size_t raw, size;
  unsigned char * uncompressed = generate_PNG_frame_data(context, data, type, &raw, boundaries);
  // if chunkID is non-null, compress_PNG_data will insert four bytes of padding before the compressed data so this function can write a chunk ID there
  unsigned char * compressed = compress_PNG_data(context, uncompressed, raw, chunkID ? 4 : 0, &size);
  ctxfree(context, uncompressed);
  unsigned char * current = compressed;
  if (chunkID) {
    current += 4;
    while (size > 0x7ffffffbu) {
      if (*chunkID > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
      write_be32_unaligned(current - 4, (*chunkID) ++);
      output_PNG_chunk(context, 0x66644154u, 0x7ffffffcu, current - 4); // fdAT
      current += 0x7ffffff8u;
      size -= 0x7ffffff8u;
    }
    if (size) {
      if (*chunkID > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
      write_be32_unaligned(current - 4, (*chunkID) ++);
      output_PNG_chunk(context, 0x66644154u, size + 4, current - 4); // fdAT
    }
  } else {
    while (size > 0x7fffffffu) {
      output_PNG_chunk(context, 0x49444154u, 0x7ffffffcu, current); // IDAT
      current += 0x7ffffffcu;
      size -= 0x7ffffffcu;
    }
    if (size) output_PNG_chunk(context, 0x49444154u, size, current); // IDAT
  }
  ctxfree(context, compressed);
}

void append_APNG_frame_header (struct context * context, uint64_t duration, uint8_t disposal, uint32_t * restrict chunkID, int64_t * restrict duration_remainder,
                               const struct plum_rectangle * boundaries) {
  if (*chunkID > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  uint32_t numerator = 0, denominator = 0;
  if (duration) {
    if (duration == 1) duration = 0;
    duration = adjust_frame_duration(duration, duration_remainder);
    calculate_frame_duration_fraction(duration, 0xffffu, &numerator, &denominator);
    if (!numerator) {
      denominator = 0;
      update_frame_duration_remainder(duration, 0, duration_remainder);
    } else {
      if (!denominator) {
        // duration too large (calculation returned infinity), so max it out
        numerator = 0xffffu;
        denominator = 1;
      }
      update_frame_duration_remainder(duration, ((uint64_t) 1000000000u * numerator + denominator / 2) / denominator, duration_remainder);
    }
  }
  unsigned char data[26];
  write_be32_unaligned(data, (*chunkID) ++);
  if (boundaries) {
    write_be32_unaligned(data + 4, boundaries -> width);
    write_be32_unaligned(data + 8, boundaries -> height);
    write_be32_unaligned(data + 12, boundaries -> left);
    write_be32_unaligned(data + 16, boundaries -> top);
  } else {
    write_be32_unaligned(data + 4, context -> source -> width);
    write_be32_unaligned(data + 8, context -> source -> height);
    memset(data + 12, 0, 8);
  }
  write_be16_unaligned(data + 20, numerator);
  write_be16_unaligned(data + 22, denominator);
  bytewrite(data + 24, disposal % PLUM_DISPOSAL_REPLACE, disposal < PLUM_DISPOSAL_REPLACE);
  output_PNG_chunk(context, 0x6663544cu, sizeof data, data); // fcTL
}

void output_PNG_chunk (struct context * context, uint32_t type, uint32_t size, const void * restrict data) {
  unsigned char * node = append_output_node(context, size + 12);
  write_be32_unaligned(node, size);
  write_be32_unaligned(node + 4, type);
  if (size) memcpy(node + 8, data, size);
  write_be32_unaligned(node + size + 8, compute_PNG_CRC(node + 4, size + 4));
}

unsigned char * generate_PNG_frame_data (struct context * context, const void * restrict data, unsigned type, size_t * restrict size,
                                         const struct plum_rectangle * boundaries) {
  struct plum_rectangle framearea;
  if (boundaries)
    framearea = *boundaries;
  else
    framearea = (const struct plum_rectangle) {.left = 0, .top = 0, .width = context -> source -> width, .height = context -> source -> height};
  size_t rowsize, pixelsize = bytes_per_channel_PNG[type];
  if (pixelsize)
    rowsize = framearea.width * pixelsize + 1;
  else
    rowsize = (((size_t) framearea.width << type) + 7) / 8 + 1;
  *size = rowsize * framearea.height;
  if (*size > SIZE_MAX - 2 || rowsize > SIZE_MAX / 6 || *size / rowsize != framearea.height) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  // allocate and initialize two extra bytes so the compressor can operate safely
  unsigned char * result = ctxcalloc(context, *size + 2);
  unsigned char * rowbuffer = ctxcalloc(context, 6 * rowsize);
  size_t rowoffset = (type >= 4) ? plum_color_buffer_size(context -> source -> width, context -> source -> color_format) : context -> source -> width;
  size_t dataoffset = (type >= 4) ? plum_color_buffer_size(framearea.left, context -> source -> color_format) : framearea.left;
  dataoffset += rowoffset * framearea.top;
  for (uint_fast32_t row = 0; row < framearea.height; row ++) {
    generate_PNG_row_data(context, (const unsigned char *) data + dataoffset + rowoffset * row, rowbuffer, framearea.width, type);
    filter_PNG_rows(rowbuffer, rowbuffer + 5 * rowsize, framearea.width, type);
    memcpy(rowbuffer + 5 * rowsize, rowbuffer, rowsize);
    memcpy(result + rowsize * row, rowbuffer + rowsize * select_PNG_filtered_row(rowbuffer, rowsize), rowsize);
  }
  ctxfree(context, rowbuffer);
  return result;
}

void generate_PNG_row_data (struct context * context, const void * restrict data, unsigned char * restrict output, size_t width, unsigned type) {
  *(output ++) = 0;
  switch (type) {
    case 0: case 1: case 2: {
      const unsigned char * indexes = data;
      uint_fast8_t dataword = 0, bits = 0, pixelbits = 1 << type;
      for (uint_fast32_t p = 0; p < width; p ++) {
        dataword = (dataword << pixelbits) | *(indexes ++);
        bits += pixelbits;
        if (bits == 8) {
          *(output ++) = dataword;
          bits = 0;
        }
      }
      if (bits) *output = dataword << (8 - bits);
    } break;
    case 3:
      memcpy(output, data, width);
      break;
    case 4: case 5: {
      uint32_t * pixels = ctxmalloc(context, sizeof *pixels * width);
      plum_convert_colors(pixels, data, width, PLUM_COLOR_32 | PLUM_ALPHA_INVERT, context -> source -> color_format);
      if (type == 5)
        for (uint_fast32_t p = 0; p < width; p ++) write_le32_unaligned(output + 4 * p, pixels[p]);
      else
        for (uint_fast32_t p = 0; p < width; p ++) output += byteappend(output, pixels[p], pixels[p] >> 8, pixels[p] >> 16);
      ctxfree(context, pixels);
    } break;
    case 6: case 7: {
      uint64_t * pixels = ctxmalloc(context, sizeof *pixels * width);
      plum_convert_colors(pixels, data, width, PLUM_COLOR_64 | PLUM_ALPHA_INVERT, context -> source -> color_format);
      if (type == 7)
        for (uint_fast32_t p = 0; p < width; p ++)
          output += byteappend(output, pixels[p] >> 8, pixels[p], pixels[p] >> 24, pixels[p] >> 16, pixels[p] >> 40, pixels[p] >> 32,
                               pixels[p] >> 56, pixels[p] >> 48);
      else
        for (uint_fast32_t p = 0; p < width; p ++)
          output += byteappend(output, pixels[p] >> 8, pixels[p], pixels[p] >> 24, pixels[p] >> 16, pixels[p] >> 40, pixels[p] >> 32);
      ctxfree(context, pixels);
    }
  }
}

void filter_PNG_rows (unsigned char * restrict rowdata, const unsigned char * restrict previous, size_t count, unsigned type) {
  ptrdiff_t rowsize, pixelsize = bytes_per_channel_PNG[type];
  // rowsize doesn't include the filter type byte
  if (pixelsize)
    rowsize = count * pixelsize;
  else {
    rowsize = ((count << type) + 7) / 8;
    pixelsize = 1; // treat packed bits as a single pixel
  }
  rowdata ++;
  previous ++;
  unsigned char * output = rowdata + rowsize;
  *(output ++) = 1;
  for (ptrdiff_t p = 0; p < pixelsize; p ++) *(output ++) = rowdata[p];
  for (ptrdiff_t p = pixelsize; p < rowsize; p ++) *(output ++) = rowdata[p] - rowdata[p - pixelsize];
  *(output ++) = 2;
  for (ptrdiff_t p = 0; p < rowsize; p ++) *(output ++) = rowdata[p] - previous[p];
  *(output ++) = 3;
  for (ptrdiff_t p = 0; p < pixelsize; p ++) *(output ++) = rowdata[p] - (previous[p] >> 1);
  for (ptrdiff_t p = pixelsize; p < rowsize; p ++) *(output ++) = rowdata[p] - ((previous[p] + rowdata[p - pixelsize]) >> 1);
  *(output ++) = 4;
  for (ptrdiff_t p = 0; p < rowsize; p ++) {
    int top = previous[p], left = (p >= pixelsize) ? rowdata[p - pixelsize] : 0, diagonal = (p >= pixelsize) ? previous[p - pixelsize] : 0;
    int topdiff = absolute_value(left - diagonal), leftdiff = absolute_value(top - diagonal), diagdiff = absolute_value(left + top - diagonal * 2);
    *(output ++) = rowdata[p] - ((leftdiff <= topdiff && leftdiff <= diagdiff) ? left : (topdiff <= diagdiff) ? top : diagonal);
  }
}

unsigned char select_PNG_filtered_row (const unsigned char * rowdata, size_t rowsize) {
  // recommended by the standard: treat each byte as signed and pick the filter that results in the smallest sum of absolute values
  // ties are broken by smallest filter number, because lower-numbered filters are simpler than higher-numbered filters
  uint_fast64_t best_score = 0;
  for (size_t p = 0; p < rowsize; p ++, rowdata ++) best_score += (*rowdata >= 0x80) ? 0x100 - *rowdata : *rowdata;
  uint_fast8_t best = 0;
  for (uint_fast8_t current = 1; current < 5; current ++) {
    uint_fast64_t current_score = 0;
    for (size_t p = 0; p < rowsize; p ++, rowdata ++) current_score += (*rowdata >= 0x80) ? 0x100 - *rowdata : *rowdata;
    if (current_score < best_score) {
      best = current;
      best_score = current_score;
    }
  }
  return best;
}
