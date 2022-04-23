#include "proto.h"

void load_GIF_data (struct context * context, unsigned flags, size_t limit) {
  if (context -> size < 14) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  context -> image -> type = PLUM_IMAGE_GIF;
  context -> image -> width = read_le16_unaligned(context -> data + 6);
  context -> image -> height = read_le16_unaligned(context -> data + 8);
  size_t offset = 13;
  uint64_t transparent = 0xffff000000000000u;
  // note: load_GIF_palettes also initializes context -> image -> frames (and context -> image -> palette) and validates the image's structure
  uint64_t ** palettes = load_GIF_palettes_and_frame_count(context, flags, &offset, &transparent); // will be leaked (collected at the end)
  validate_image_size(context, limit);
  allocate_framebuffers(context, flags, !!(context -> image -> palette));
  uint64_t * durations;
  uint8_t * disposals;
  add_animation_metadata(context, &durations, &disposals);
  for (uint_fast32_t frame = 0; frame < context -> image -> frames; frame ++)
    load_GIF_frame(context, &offset, flags, frame, palettes ? palettes[frame] : NULL, transparent, durations + frame, disposals + frame);
  if (!plum_find_metadata(context -> image, PLUM_METADATA_LOOP_COUNT)) add_loop_count_metadata(context, 1);
}

uint64_t ** load_GIF_palettes_and_frame_count (struct context * context, unsigned flags, size_t * offset, uint64_t * transparent_color) {
  // will also validate block order
  unsigned char depth = 1 + ((context -> data[10] >> 4) & 7);
  add_color_depth_metadata(context, depth, depth, depth, 1, 0);
  uint64_t * global_palette = ctxcalloc(context, 256 * sizeof *global_palette);
  unsigned global_palette_size = 0;
  if (context -> data[10] & 0x80) {
    global_palette_size = 2 << (context -> data[10] & 7);
    load_GIF_palette(context, global_palette, offset, global_palette_size);
    if (context -> data[11] < global_palette_size) {
      add_background_color_metadata(context, global_palette[context -> data[11]], flags);
      *transparent_color |= global_palette[context -> data[11]];
    }
  }
  size_t scan_offset = *offset;
  unsigned real_global_palette_size = global_palette_size, transparent_index = 256, next_transparent_index = 256, seen_extension = 0;
  uint64_t ** result = NULL;
  while (scan_offset < context -> size)
    switch (context -> data[scan_offset ++]) {
      case 0x21: {
        if (scan_offset == context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        uint_fast8_t exttype = context -> data[scan_offset ++];
        if (exttype == 0xf9) {
          if (seen_extension) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          seen_extension = 1;
          size_t extsize;
          unsigned char * extdata = load_GIF_data_blocks(context, &scan_offset, &extsize);
          if (extsize != 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          if (*extdata & 1)
            next_transparent_index = extdata[3];
          else
            next_transparent_index = 256;
          ctxfree(context, extdata);
        } else if (exttype < 0x80)
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        else
          skip_GIF_data_blocks(context, &scan_offset);
      } break;
      case 0x2c: {
        if (scan_offset > context -> size - 9) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        scan_offset += 9;
        context -> image -> frames ++;
        if (!context -> image -> frames) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
        int smaller_size = read_le16_unaligned(context -> data + scan_offset - 9) || read_le16_unaligned(context -> data + scan_offset - 7) ||
                           read_le16_unaligned(context -> data + scan_offset - 5) != context -> image -> width ||
                           read_le16_unaligned(context -> data + scan_offset - 3) != context -> image -> height;
        uint64_t * local_palette = ctxmalloc(context, 256 * sizeof *local_palette);
        unsigned local_palette_size = 2 << (context -> data[scan_offset - 1] & 7);
        if (context -> data[scan_offset - 1] & 0x80)
          load_GIF_palette(context, local_palette, &scan_offset, local_palette_size);
        else
          local_palette_size = 0;
        if (!(local_palette_size || real_global_palette_size)) throw(context, PLUM_ERR_UNDEFINED_PALETTE);
        if (next_transparent_index < (local_palette_size ? local_palette_size : real_global_palette_size))
          local_palette[next_transparent_index] = *transparent_color;
        else
          next_transparent_index = 256;
        if (transparent_index == 256) transparent_index = next_transparent_index;
        if (global_palette_size && !result) {
          // check if the current palette is compatible with the global one; if so, don't add any per-frame palettes
          if (!(smaller_size && next_transparent_index == 256) && transparent_index == next_transparent_index) {
            if (!local_palette_size) goto added;
            unsigned min = (local_palette_size < global_palette_size) ? local_palette_size : global_palette_size;
            // temporarily reset this location so it won't fail the check on that spot
            if (next_transparent_index < min) local_palette[next_transparent_index] = global_palette[next_transparent_index];
            int palcheck = memcmp(local_palette, global_palette, min * sizeof *global_palette);
            if (next_transparent_index < min) local_palette[next_transparent_index] = *transparent_color;
            if (!palcheck) {
              if (local_palette_size > global_palette_size) {
                memcpy(global_palette + global_palette_size, local_palette + global_palette_size,
                       (local_palette_size - global_palette_size) * sizeof *global_palette);
                global_palette_size = local_palette_size;
              }
              goto added;
            }
          }
          // palettes are incompatible: break down the current global palette into per-frame copies
          if (context -> image -> frames) {
            result = ctxmalloc(context, (context -> image -> frames - 1) * sizeof *result);
            uint64_t * palcopy = ctxcalloc(context, 256 * sizeof *palcopy);
            // it doesn't matter that the pointer is reused, because it won't be freed explicitly
            for (uint_fast32_t p = 0; p < context -> image -> frames - 1; p ++) result[p] = palcopy;
            memcpy(palcopy, global_palette, global_palette_size * sizeof *palcopy);
            if (transparent_index < global_palette_size) palcopy[transparent_index] = *transparent_color;
          }
        }
        result = ctxrealloc(context, result, context -> image -> frames * sizeof *result);
        result[context -> image -> frames - 1] = ctxcalloc(context, 256 * sizeof **result);
        if (local_palette_size)
          memcpy(result[context -> image -> frames - 1], local_palette, local_palette_size * sizeof **result);
        else {
          memcpy(result[context -> image -> frames - 1], global_palette, global_palette_size * sizeof **result);
          if (next_transparent_index < global_palette_size)
            result[context -> image -> frames - 1][next_transparent_index] = *transparent_color;
        }
        // either the frame palette has been added to the per-frame list or the global palette is still in use
        added:
        ctxfree(context, local_palette);
        scan_offset ++;
        if (scan_offset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        skip_GIF_data_blocks(context, &scan_offset);
        next_transparent_index = 256;
        seen_extension = 0;
      } break;
      case 0x3b:
        if (!seen_extension) goto done;
      default:
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
  throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  done:
  if (!context -> image -> frames) throw(context, PLUM_ERR_NO_DATA);
  if (!result) {
    if (transparent_index < global_palette_size) global_palette[transparent_index] = *transparent_color;
    context -> image -> max_palette_index = global_palette_size - 1;
    context -> image -> palette = plum_malloc(context -> image, plum_color_buffer_size(global_palette_size, flags));
    if (!context -> image -> palette) throw(context, PLUM_ERR_OUT_OF_MEMORY);
    plum_convert_colors(context -> image -> palette, global_palette, global_palette_size, flags, PLUM_COLOR_64);
  }
  ctxfree(context, global_palette);
  return result;
}

void load_GIF_palette (struct context * context, uint64_t * palette, size_t * offset, unsigned size) {
  if (3 * size > context -> size - *offset) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  while (size --) {
    uint_fast64_t color = context -> data[(*offset) ++];
    color |= (uint_fast64_t) context -> data[(*offset) ++] << 16;
    color |= (uint_fast64_t) context -> data[(*offset) ++] << 32;
    *(palette ++) = color * 0x101;
  }
}

void * load_GIF_data_blocks (struct context * context, size_t * restrict offset, size_t * restrict loaded_size) {
  if (*offset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t current_size = 0;
  for (size_t block, p = *offset; block = context -> data[p ++]; current_size += block) {
    p += block;
    if (p >= context -> size || p < block) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  *loaded_size = current_size;
  unsigned char * result = ctxmalloc(context, current_size);
  for (size_t block, copied_size = 0; block = context -> data[(*offset) ++]; copied_size += block) {
    memcpy(result + copied_size, context -> data + *offset, block);
    *offset += block;
  }
  return result;
}

void skip_GIF_data_blocks (struct context * context, size_t * offset) {
  uint_fast8_t skip;
  do {
    if (*offset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    skip = context -> data[(*offset) ++];
    if (context -> size < skip || *offset > context -> size - skip) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    *offset += skip;
  } while (skip);
}

void load_GIF_frame (struct context * context, size_t * offset, unsigned flags, uint32_t frame, const uint64_t * palette,
                     uint64_t transparent_color, uint64_t * restrict duration, uint8_t * restrict disposal) {
  *duration = *disposal = 0;
  int transparent_index = -1;
  // frames have already been validated, so at this point, we can only have extensions (0x21 ID block block block...) or image descriptors
  while (context -> data[(*offset) ++] == 0x21) {
    unsigned char extkind = context -> data[(*offset) ++];
    if (extkind != 0xf9 && extkind != 0xff) {
      skip_GIF_data_blocks(context, offset);
      continue;
    }
    size_t extsize;
    unsigned char * extdata = load_GIF_data_blocks(context, offset, &extsize);
    if (extkind == 0xff) {
      if (extsize == 14 && bytematch(extdata, 0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x01)) {
        if (plum_find_metadata(context -> image, PLUM_METADATA_LOOP_COUNT)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        add_loop_count_metadata(context, read_le16_unaligned(extdata + 12));
      }
    } else {
      *duration = (uint64_t) 10000000 * read_le16_unaligned(extdata + 1);
      uint_fast8_t dispindex = (*extdata >> 2) & 7;
      if (dispindex > 3) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (dispindex) *disposal = dispindex - 1;
      if (*extdata & 1) transparent_index = extdata[3];
    }
    ctxfree(context, extdata);
  }
  if (!*duration) *duration = 1;
  uint_fast32_t left = read_le16_unaligned(context -> data + *offset);
  uint_fast32_t top = read_le16_unaligned(context -> data + *offset + 2);
  uint_fast32_t width = read_le16_unaligned(context -> data + *offset + 4);
  uint_fast32_t height = read_le16_unaligned(context -> data + *offset + 6);
  if (left + width > context -> image -> width || top + height > context -> image -> height) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast8_t frameflags = context -> data[*offset + 8];
  *offset += 9;
  uint8_t max_palette_index;
  if (frameflags & 0x80) {
    *offset += 6 << (frameflags & 7);
    max_palette_index = (2 << (frameflags & 7)) - 1;
  } else
    max_palette_index = (2 << (context -> data[10] & 7)) - 1;
  uint8_t codesize = context -> data[(*offset) ++];
  if (codesize < 2 || codesize > 11) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t length;
  unsigned char * compressed = load_GIF_data_blocks(context, offset, &length);
  unsigned char * buffer = ctxmalloc(context, (size_t) width * height);
  decompress_GIF_data(context, buffer, compressed, width * height, length, codesize);
  ctxfree(context, compressed);
  if (frameflags & 0x40) deinterlace_GIF_frame(context, buffer, width, height);
  for (size_t p = 0; p < width * height; p ++) if (buffer[p] > max_palette_index) throw(context, PLUM_ERR_INVALID_COLOR_INDEX);
  if (width == context -> image -> width && height == context -> image -> height)
    write_palette_framebuffer_to_image(context, buffer, palette, frame, flags, 0xff);
  else if (context -> image -> palette) {
    if (transparent_index < 0) throw(context, PLUM_ERR_INVALID_FILE_FORMAT); // if we got here somehow, it's irrecoverable
    uint8_t * fullframe = ctxmalloc(context, context -> image -> width * context -> image -> height);
    memset(fullframe, transparent_index, context -> image -> width * context -> image -> height);
    for (uint_fast16_t row = top; row < top + height; row ++)
      memcpy(fullframe + context -> image -> width * row + left, buffer + width * (row - top), width);
    write_palette_framebuffer_to_image(context, fullframe, palette, frame, flags, 0xff);
    ctxfree(context, fullframe);
  } else {
    uint64_t * fullframe = ctxmalloc(context, sizeof *fullframe * context -> image -> width * context -> image -> height);
    uint64_t * current = fullframe;
    for (uint_fast16_t row = 0; row < top; row ++)
      for (uint_fast16_t col = 0; col < context -> image -> width; col ++) *(current ++) = transparent_color;
    for (uint_fast16_t row = top; row < top + height; row ++) {
      for (uint_fast16_t col = 0; col < left; col ++) *(current ++) = transparent_color;
      for (uint_fast16_t col = left; col < left + width; col ++) *(current ++) = palette[buffer[(row - top) * width + col - left]];
      for (uint_fast16_t col = left + width; col < context -> image -> width; col ++) *(current ++) = transparent_color;
    }
    for (uint_fast16_t row = top + height; row < context -> image -> height; row ++)
      for (uint_fast16_t col = 0; col < context -> image -> width; col ++) *(current ++) = transparent_color;
    write_framebuffer_to_image(context -> image, fullframe, frame, flags);
    ctxfree(context, fullframe);
  }
  ctxfree(context, buffer);
}

void deinterlace_GIF_frame (struct context * context, unsigned char * restrict buffer, uint16_t width, uint16_t height) {
  unsigned char * temp = ctxmalloc(context, (size_t) width * height);
  uint_fast32_t target = 0;
  for (uint_fast32_t row = 0; row < height; row += 8) memcpy(temp + row * width, buffer + (target ++) * width, width);
  for (uint_fast32_t row = 4; row < height; row += 8) memcpy(temp + row * width, buffer + (target ++) * width, width);
  for (uint_fast32_t row = 2; row < height; row += 4) memcpy(temp + row * width, buffer + (target ++) * width, width);
  for (uint_fast32_t row = 1; row < height; row += 2) memcpy(temp + row * width, buffer + (target ++) * width, width);
  memcpy(buffer, temp, (size_t) width * height);
  ctxfree(context, temp);
}
