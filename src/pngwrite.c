#include "proto.h"

void generate_PNG_data (struct context * context) {
  if (context -> source -> frames > 1) throw(context, PLUM_ERR_NO_MULTI_FRAME);
  unsigned type = generate_PNG_header(context);
  append_PNG_image_data(context, context -> source -> data, type, NULL);
  output_PNG_chunk(context, 0x49454e44u, 0, NULL); // IEND
}

void generate_APNG_data (struct context * context) {
  if (context -> source -> frames > 0x40000000u) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  unsigned type = generate_PNG_header(context);
  uint32_t loops = 1;
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_LOOP_COUNT);
  if (metadata) {
    loops = *(uint32_t *) metadata -> data;
    if (loops > 0x7fffffffu) throw(context, PLUM_ERR_INVALID_METADATA);
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
  uint_fast8_t disposal, last_disposal = (disposal_count >= context -> source -> frames) ? disposals[context -> source -> frames - 1] : 0;
  unsigned char animation_data[8];
  write_be32_unaligned(animation_data + 4, loops);
  if ((duration_count && *durations) || (context -> source -> frames == 1)) {
    write_be32_unaligned(animation_data, context -> source -> frames);
    output_PNG_chunk(context, 0x6163544cu, sizeof animation_data, animation_data); // acTL
    disposal = disposal_count ? *disposals : 0;
    append_APNG_frame_header(context, duration_count ? *durations : 0, disposal, last_disposal, &chunkID);
    last_disposal = disposal;
  } else {
    write_be32_unaligned(animation_data, context -> source -> frames - 1);
    output_PNG_chunk(context, 0x6163544cu, sizeof animation_data, animation_data); // acTL
  }
  append_PNG_image_data(context, context -> source -> data, type, NULL);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  if (!context -> source -> palette) framesize = plum_color_buffer_size(framesize, context -> source -> color_format);
  uint_fast32_t frame;
  for (frame = 1; frame < context -> source -> frames; frame ++) {
    disposal = (disposal_count > frame) ? disposals[frame] : 0;
    append_APNG_frame_header(context, (duration_count > frame) ? durations[frame] : 0, disposal, last_disposal, &chunkID);
    last_disposal = disposal;
    append_PNG_image_data(context, context -> source -> data8 + framesize * frame, type, &chunkID);
  }
  output_PNG_chunk(context, 0x49454e44u, 0, NULL); // IEND
}

unsigned generate_PNG_header (struct context * context) {
  // returns the selected type of image: 0, 1, 2, 3: paletted (1 << type bits), 4, 5: 8-bit RGB (without and with alpha), 6, 7: 16-bit RGB
  byteoutput(context, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a);
  unsigned type;
  uint32_t depth = get_true_color_depth(context -> source);
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
    type = 4 + (depth >= 0x1000000u);
  else
    type = 6 + (depth >= 0x1000000u);
  append_PNG_header_chunks(context, type, depth);
  if (type < 4) append_PNG_palette_data(context, depth >= 0x1000000u);
  const struct plum_metadata * background = plum_find_metadata(context -> source, PLUM_METADATA_BACKGROUND);
  if (background) append_PNG_background_chunk(context, background -> data, type);
  return type;
}

void append_PNG_header_chunks (struct context * context, unsigned type, uint32_t depth) {
  if ((context -> source -> width > 0x7fffffffu) || (context -> source -> height > 0x7fffffffu)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  unsigned char header[13];
  write_be32_unaligned(header, context -> image -> width);
  write_be32_unaligned(header + 4, context -> image -> height);
  header[8] = (type < 4) ? 1 << type : (8 << (type >= 6));
  header[9] = (type >= 4) ? 2 + 4 * (type & 1) : 3;
  header[10] = header[11] = header[12] = 0;
  output_PNG_chunk(context, 0x49484452u, sizeof header, header); // IHDR
  // reuse the header array for the sBIT chunk
  write_le32_unaligned(header, depth); // this will write each byte of depth in the expected position
  if (type < 4) {
    if (*header > 8) *header = 8;
    if (header[1] > 8) header[1] = 8;
    if (header[2] > 8) header[2] = 8;
  }
  output_PNG_chunk(context, 0x73424954u, 3 + ((type == 5) || (type == 7)), header); // sBIT
}

void append_PNG_palette_data (struct context * context, int use_alpha) {
  uint32_t color_buffer[256];
  plum_convert_colors(color_buffer, context -> source -> palette, context -> source -> max_palette_index + 1, PLUM_COLOR_32 | PLUM_ALPHA_INVERT,
                      context -> source -> color_format);
  unsigned char data[768];
  unsigned char * p = data;
  uint_fast16_t index;
  for (index = 0; index <= context -> source -> max_palette_index; index ++) {
    *(p ++) = color_buffer[index];
    *(p ++) = color_buffer[index] >> 8;
    *(p ++) = color_buffer[index] >> 16;
  }
  output_PNG_chunk(context, 0x504c5445u, p - data, data); // PLTE
  if (use_alpha) {
    p = data;
    for (index = 0; index <= context -> source -> max_palette_index; index ++)
      *(p ++) = color_buffer[index] >> 24;
    output_PNG_chunk(context, 0x74524e53u, p - data, data); // tRNS
  }
}

void append_PNG_background_chunk (struct context * context, const void * restrict data, unsigned type) {
  if (type >= 4) {
    unsigned char data[6];
    uint64_t color;
    plum_convert_colors(&color, data, 1, PLUM_COLOR_64, context -> source -> color_format);
    if (type < 6) color = (color >> 8) & 0xff00ff00ffu;
    write_be16_unaligned(data, color);
    write_be16_unaligned(data + 2, color >> 16);
    write_be16_unaligned(data + 4, color >> 32);
    output_PNG_chunk(context, 0x624b4744u, sizeof data, data); // bKGD
  } else {
    unsigned pos, size = plum_color_buffer_size(1, context -> source -> color_format);
    const unsigned char * current = context -> source -> palette;
    for (pos = 0; pos <= context -> source -> max_palette_index; pos ++, current += size) if (!memcmp(current, data, size)) {
      unsigned char byte = pos;
      output_PNG_chunk(context, 0x624b4744u, 1, &byte); // bKGD
      return;
    }
  }
}

void append_PNG_image_data (struct context * context, const void * restrict data, unsigned type, uint32_t * restrict chunkID) {
  // chunkID counts animation data chunks (fcTL, fdAT); if chunkID is null, emit IDAT chunks instead
  size_t raw, size;
  unsigned char * uncompressed = generate_PNG_frame_data(context, data, type, &raw);
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

void append_APNG_frame_header (struct context * context, uint64_t duration, uint8_t disposal, uint8_t previous, uint32_t * restrict chunkID) {
  if (*chunkID > 0x7fffffffu) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  uint32_t numerator, denominator;
  calculate_frame_duration_fraction(duration, 0xffffu, &numerator, &denominator);
  if (!denominator) throw(context, PLUM_ERR_INVALID_METADATA); // duration too large -- calculation returned infinity
  if (!numerator) denominator = 0;
  unsigned char data[26];
  write_be32_unaligned(data, (*chunkID) ++);
  write_be32_unaligned(data + 4, context -> source -> width);
  write_be32_unaligned(data + 8, context -> source -> height);
  memset(data + 12, 0, 8);
  write_be16_unaligned(data + 20, numerator);
  write_be16_unaligned(data + 22, denominator);
  data[24] = disposal % PLUM_DISPOSAL_REPLACE;
  data[25] = previous < PLUM_DISPOSAL_REPLACE;
  output_PNG_chunk(context, 0x6663544cu, sizeof data, data); // fcTL
}

void output_PNG_chunk (struct context * context, uint32_t type, uint32_t size, const void * restrict data) {
  unsigned char * node = append_output_node(context, size + 12);
  write_be32((uint32_t *) node, size);
  write_be32((uint32_t *) (node + 4), type);
  if (size) memcpy(node + 8, data, size);
  write_be32_unaligned(node + size + 8, compute_PNG_CRC(node + 4, size + 4));
}

unsigned char * generate_PNG_frame_data (struct context * context, const void * restrict data, unsigned type, size_t * restrict size) {
  size_t rowsize, pixelsize = type[(const size_t []) {0, 0, 0, 1, 3, 4, 6, 8}];
  if (pixelsize)
    rowsize = context -> source -> width * pixelsize + 1;
  else
    rowsize = (((size_t) context -> source -> width << type) + 15) >> 3;
  *size = rowsize * context -> source -> height;
  // allocate and initialize two extra bytes so the compressor can operate safely
  unsigned char * result = ctxmalloc(context, *size + 2);
  result[*size] = 0;
  result[*size + 1] = 0;
  unsigned char * rowbuffer = ctxmalloc(context, 6 * rowsize);
  memset(rowbuffer + 5 * rowsize, 0, rowsize);
  uint_fast32_t row;
  size_t rowoffset = (type < 4) ? context -> source -> width : plum_color_buffer_size(context -> source -> width, context -> source -> color_format);
  for (row = 0; row < context -> source -> height; row ++) {
    generate_PNG_row_data(context, (const unsigned char *) data + rowoffset * row, rowbuffer, type);
    filter_PNG_rows(rowbuffer, rowbuffer + 5 * rowsize, context -> source -> width, type);
    memcpy(rowbuffer + 5 * rowsize, rowbuffer, rowsize);
    memcpy(result + rowsize * row, rowbuffer + rowsize * select_PNG_filtered_row(rowbuffer, rowsize), rowsize);
  }
  ctxfree(context, rowbuffer);
  return result;
}

void generate_PNG_row_data (struct context * context, const void * restrict data, unsigned char * restrict output, unsigned type) {
  *(output ++) = 0;
  uint_fast32_t p;
  switch (type) {
    case 0: case 1: case 2: {
      const unsigned char * indexes = data;
      uint_fast8_t dataword = 0, bits = 0, pixelbits = 1 << type;
      for (p = 0; p < context -> source -> width; p ++) {
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
      memcpy(output, data, context -> source -> width);
      break;
    case 4: case 5: {
      uint32_t * pixels = ctxmalloc(context, sizeof *pixels * context -> source -> width);
      plum_convert_colors(pixels, data, context -> source -> width, PLUM_COLOR_32 | PLUM_ALPHA_INVERT, context -> source -> color_format);
      for (p = 0; p < context -> source -> width; p ++) {
        *(output ++) = pixels[p];
        *(output ++) = pixels[p] >> 8;
        *(output ++) = pixels[p] >> 16;
        if (type == 5) *(output ++) = pixels[p] >> 24;
      }
      ctxfree(context, pixels);
    } break;
    case 6: case 7: {
      uint64_t * pixels = ctxmalloc(context, sizeof *pixels * context -> source -> width);
      plum_convert_colors(pixels, data, context -> source -> width, PLUM_COLOR_64 | PLUM_ALPHA_INVERT, context -> source -> color_format);
      for (p = 0; p < context -> source -> width; p ++) {
        *(output ++) = pixels[p] >> 8;
        *(output ++) = pixels[p];
        *(output ++) = pixels[p] >> 24;
        *(output ++) = pixels[p] >> 16;
        *(output ++) = pixels[p] >> 40;
        *(output ++) = pixels[p] >> 32;
        if (type == 7) {
          *(output ++) = pixels[p] >> 56;
          *(output ++) = pixels[p] >> 48;
        }
      }
      ctxfree(context, pixels);
    }
  }
}

void filter_PNG_rows (unsigned char * restrict rowdata, const unsigned char * restrict previous, size_t count, unsigned type) {
  uint_fast8_t pixelsize = type[(const unsigned char []) {1, 1, 1, 1, 3, 4, 6, 8}];
  if (type < 3) count = ((count << type) + 7) >> 3;
  ptrdiff_t p, rowsize = count * pixelsize; // rowsize doesn't include the filter type byte
  rowdata ++;
  previous ++;
  unsigned char * output = rowdata + rowsize;
  *(output ++) = 1;
  for (p = 0; p < pixelsize; p ++) *(output ++) = rowdata[p];
  for (; p < rowsize; p ++) *(output ++) = rowdata[p] - rowdata[p - pixelsize];
  *(output ++) = 2;
  for (p = 0; p < rowsize; p ++) *(output ++) = rowdata[p] - previous[p];
  *(output ++) = 3;
  for (p = 0; p < pixelsize; p ++) *(output ++) = rowdata[p] - (previous[p] >> 1);
  for (; p < rowsize; p ++) *(output ++) = rowdata[p] - ((previous[p] + rowdata[p - pixelsize]) >> 1);
  *(output ++) = 4;
  for (p = 0; p < rowsize; p ++) {
    int top = previous[p], left = (p >= pixelsize) ? rowdata[p - pixelsize] : 0, diagonal = (p >= pixelsize) ? previous[p - pixelsize] : 0;
    int topdiff = absolute_value(left - diagonal), leftdiff = absolute_value(top - diagonal), diagdiff = absolute_value(left + top - diagonal * 2);
    *(output ++) = rowdata[p] - (((leftdiff <= topdiff) && (leftdiff <= diagdiff)) ? left : (topdiff <= diagdiff) ? top : diagonal);
  }
}

unsigned char select_PNG_filtered_row (const unsigned char * rowdata, size_t rowsize) {
  // recommended by the standard: treat each byte as signed and pick the filter that results in the smallest sum of absolute values
  // ties are broken by smallest filter number, because lower-numbered filters are simpler than higher-numbered filters
  uint_fast8_t current, best = 0;
  uint_fast64_t current_score, best_score = 0;
  size_t p;
  for (p = 0; p < rowsize; p ++, rowdata ++) best_score += (*rowdata >= 0x80) ? 0x100 - *rowdata : *rowdata;
  for (current = 1; current < 5; current ++) {
    current_score = 0;
    for (p = 0; p < rowsize; p ++, rowdata ++) current_score += (*rowdata >= 0x80) ? 0x100 - *rowdata : *rowdata;
    if (current_score < best_score) {
      best = current;
      best_score = current_score;
    }
  }
  return best;
}
