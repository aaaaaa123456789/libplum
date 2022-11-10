#include "proto.h"

void load_PNG_data (struct context * context, unsigned long flags, size_t limit) {
  struct PNG_chunk_locations * chunks = load_PNG_chunk_locations(context); // also sets context -> image -> frames for APNGs
  // load basic header data
  if (chunks -> animation) {
    context -> image -> type = PLUM_IMAGE_APNG;
    if (*chunks -> data < *chunks -> frameinfo) context -> image -> frames ++; // first frame is not part of the animation
  } else {
    context -> image -> type = PLUM_IMAGE_PNG;
    context -> image -> frames = 1;
  }
  context -> image -> width = read_be32_unaligned(context -> data + 16);
  context -> image -> height = read_be32_unaligned(context -> data + 20);
  if (context -> image -> width > 0x7fffffffu || context -> image -> height > 0x7fffffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  validate_image_size(context, limit);
  int interlaced = context -> data[28];
  unsigned char bitdepth = context -> data[24], imagetype = context -> data[25];
  if (context -> data[26] || context -> data[27] || interlaced > 1 || imagetype > 6 || imagetype == 1 || imagetype == 5 || !bitdepth ||
      (bitdepth & (bitdepth - 1)) || bitdepth > 16 || (imagetype == 3 && bitdepth == 16) || (imagetype && imagetype != 3 && bitdepth < 8))
    throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // load palette and color-related metadata
  uint64_t * palette = NULL;
  uint8_t max_palette_index = 0;
  if (chunks -> palette && (!imagetype || imagetype == 4)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (imagetype == 3) {
    palette = ctxcalloc(context, 256 * sizeof *palette);
    max_palette_index = load_PNG_palette(context, chunks, bitdepth, palette);
  }
  add_PNG_bit_depth_metadata(context, chunks, imagetype, bitdepth);
  uint64_t background = add_PNG_background_metadata(context, chunks, palette, imagetype, bitdepth, max_palette_index, flags);
  uint64_t transparent = 0xffffffffffffffffu;
  if (chunks -> transparency)
    if (imagetype <= 2)
      transparent = load_PNG_transparent_color(context, chunks -> transparency, imagetype, bitdepth);
    else if (imagetype >= 4)
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // if there are no reduced APNG frames (i.e., frames that are smaller than the image), and we have a palette, load it into the struct
  if (palette && !(chunks -> animation && check_PNG_reduced_frames(context, chunks))) {
    context -> image -> max_palette_index = max_palette_index;
    context -> image -> palette = plum_malloc(context -> image, plum_color_buffer_size(max_palette_index + 1, flags));
    if (!context -> image -> palette) throw(context, PLUM_ERR_OUT_OF_MEMORY);
    plum_convert_colors(context -> image -> palette, palette, max_palette_index + 1, flags, PLUM_COLOR_64);
  }
  // allocate space for the image data and load the main image; for a PNG file, we're done here
  allocate_framebuffers(context, flags, context -> image -> palette);
  load_PNG_frame(context, chunks -> data, 0, palette, max_palette_index, imagetype, bitdepth, interlaced, background, transparent);
  if (!chunks -> animation) return;
  // load the animation control chunk and duration and disposal metadata
  uint32_t loops = read_be32_unaligned(context -> data + chunks -> animation + 4);
  if (loops > 0x7fffffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  add_loop_count_metadata(context, loops);
  uint64_t * durations;
  uint8_t * disposals;
  add_animation_metadata(context, &durations, &disposals);
  struct plum_rectangle * frameareas = add_frame_area_metadata(context);
  const size_t * frameinfo = chunks -> frameinfo;
  const size_t * const * framedata = (const size_t * const *) chunks -> framedata;
  // handle the first frame's metadata, which is special and may or may not be part of the animation (the frame data will have already been loaded)
  if (!*frameinfo) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (*frameinfo < *chunks -> data) {
    if (
      read_be32_unaligned(context -> data + *frameinfo + 4) != context -> image -> width ||
      read_be32_unaligned(context -> data + *frameinfo + 8) != context -> image -> height ||
      !bytematch(context -> data + *frameinfo + 12, 0, 0, 0, 0, 0, 0, 0, 0)
    ) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (**framedata) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    load_PNG_animation_frame_metadata(context, *frameinfo, durations, disposals);
    frameinfo ++;
    framedata ++;
  } else {
    *disposals = PLUM_DISPOSAL_PREVIOUS;
    *durations = 0;
  }
  *frameareas = (struct plum_rectangle) {.left = 0, .top = 0, .width = context -> image -> width, .height = context -> image -> height};
  // actually load animation frames
  if (*frameinfo && *frameinfo < *chunks -> data) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  for (uint_fast32_t frame = 1; frame < context -> image -> frames; frame ++) {
    load_PNG_animation_frame_metadata(context, *frameinfo, durations + frame, disposals + frame);
    uint_fast32_t width = read_be32_unaligned(context -> data + *frameinfo + 4);
    uint_fast32_t height = read_be32_unaligned(context -> data + *frameinfo + 8);
    uint_fast32_t left = read_be32_unaligned(context -> data + *frameinfo + 12);
    uint_fast32_t top = read_be32_unaligned(context -> data + *frameinfo + 16);
    if ((width | height | left | top) & 0x80000000u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (width + left > context -> image -> width || height + top > context -> image -> height) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    frameareas[frame] = (struct plum_rectangle) {.left = left, .top = top, .width = width, .height = height};
    if (width == context -> image -> width && height == context -> image -> height)
      load_PNG_frame(context, *framedata, frame, palette, max_palette_index, imagetype, bitdepth, interlaced, background, transparent);
    else {
      uint64_t * output = ctxmalloc(context, sizeof *output * context -> image -> width * context -> image -> height);
      uint64_t * current = output;
      size_t index = 0;
      if (palette) {
        uint8_t * pixels = load_PNG_frame_part(context, *framedata, max_palette_index, imagetype, bitdepth, interlaced, width, height, 4);
        for (size_t row = 0; row < context -> image -> height; row ++) for (size_t col = 0; col < context -> image -> width; col ++)
          if (row < top || col < left || row >= top + height || col >= left + width)
            *(current ++) = background | 0xffff000000000000u;
          else
            *(current ++) = palette[pixels[index ++]];
        ctxfree(context, pixels);
      } else {
        uint64_t * pixels = load_PNG_frame_part(context, *framedata, -1, imagetype, bitdepth, interlaced, width, height, 4);
        for (size_t row = 0; row < context -> image -> height; row ++) for (size_t col = 0; col < context -> image -> width; col ++)
          if (row < top || col < left || row >= top + height || col >= left + width)
            *(current ++) = background | 0xffff000000000000u;
          else {
            *current = pixels[index ++];
            if (transparent != 0xffffffffffffffffu && *current == transparent) *current = background | 0xffff000000000000u;
            current ++;
          }
        ctxfree(context, pixels);
      }
      write_framebuffer_to_image(context -> image, output, frame, flags);
      ctxfree(context, output);
    }
    frameinfo ++;
    framedata ++;
  }
  if (*chunks -> frameinfo >= *chunks -> data && disposals[context -> image -> frames - 1] >= PLUM_DISPOSAL_REPLACE) *disposals += PLUM_DISPOSAL_REPLACE;
  // we're done; a few things will be leaked here (chunk data, palette data...), but they are small and will be collected later
}

struct PNG_chunk_locations * load_PNG_chunk_locations (struct context * context) {
  if (context -> size < 45) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!bytematch(context -> data + 12, 0x49, 0x48, 0x44, 0x52)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t offset = 8;
  uint32_t chunk_type = 0;
  struct PNG_chunk_locations * result = ctxmalloc(context, sizeof *result);
  *result = (struct PNG_chunk_locations) {0}; // ensure that integers and pointers are properly zero-initialized
  size_t data_count = 0, frameinfo_count = 0, framedata_count = 0;
  size_t * framedata = NULL;
  bool invalid_animation = false;
  while (offset <= context -> size - 12) {
    uint32_t length = read_be32_unaligned(context -> data + offset);
    chunk_type = read_be32_unaligned(context -> data + offset + 4);
    offset += 8;
    if (length > 0x7fffffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (offset + length + 4 < offset || offset + length + 4 > context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (read_be32_unaligned(context -> data + offset + length) != compute_PNG_CRC(context -> data + offset - 4, length + 4))
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    switch (chunk_type) {
      case 0x49484452u: // IHDR
        if (offset != 16 || length != 13) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        break;
      case 0x49454e44u: // IEND
        if (length) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        offset += 4;
        goto done;
      case 0x504c5445u: // PLTE
        if (result -> palette || length % 3 || length > 0x300 || !length) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        result -> palette = offset;
        break;
      case 0x49444154u: // IDAT
        // we don't really care if they are consecutive or not; this error is easy to tolerate
        append_PNG_chunk_location(context, &result -> data, offset, &data_count);
        break;
      case 0x73424954u: // sBIT
        if (result -> bits || !length || length > 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        result -> bits = offset;
        break;
      case 0x624b4744u: // bKGD
        if (result -> background || !length || length > 6) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        result -> background = offset;
        break;
      case 0x74524e53u: // tRNS
        if (result -> transparency || length > 256) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        result -> transparency = offset;
        break;
      case 0x6163544cu: // acTL
        if (!invalid_animation)
          if (result -> data || result -> animation || length != 8)
            invalid_animation = true;
          else
            result -> animation = offset;
        break;
      case 0x6663544cu: // fcTL
        if (!invalid_animation)
          if (length == 26)
            append_PNG_chunk_location(context, &result -> frameinfo, offset, &frameinfo_count);
          else
            invalid_animation = true;
        break;
      case 0x66644154u: // fdAT
        if (!invalid_animation)
          if (length >= 4)
            append_PNG_chunk_location(context, &framedata, offset, &framedata_count);
          else
            invalid_animation = true;
        break;
      default:
        if ((chunk_type & 0xe0c0c0c0u) != 0x60404040u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT); // invalid or critical
        while (chunk_type) {
          if (!(chunk_type & 0x1f) || (chunk_type & 0x1f) > 26) throw(context, PLUM_ERR_INVALID_FILE_FORMAT); // invalid
          chunk_type >>= 8;
        }
    }
    offset += length + 4;
  }
  done:
  if (offset != context -> size || chunk_type != 0x49454e44u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!result -> data) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  append_PNG_chunk_location(context, &result -> data, 0, &data_count);
  append_PNG_chunk_location(context, &result -> frameinfo, 0, &frameinfo_count);
  frameinfo_count --;
  if (invalid_animation) {
    ctxfree(context, result -> frameinfo);
    result -> animation = 0;
    result -> frameinfo = NULL;
  } else if (result -> animation) {
    // validate and initialize frame counts here to avoid having to count them up later
    if (frameinfo_count != read_be32_unaligned(context -> data + result -> animation)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    sort_PNG_animation_chunks(context, result, framedata, frameinfo_count, framedata_count);
    context -> image -> frames = frameinfo_count;
  }
  ctxfree(context, framedata);
  return result;
}

void append_PNG_chunk_location (struct context * context, size_t ** locations, size_t location, size_t * restrict count) {
  *locations = ctxrealloc(context, *locations, sizeof **locations * (*count + 1));
  (*locations)[(*count) ++] = location;
}

void sort_PNG_animation_chunks (struct context * context, struct PNG_chunk_locations * restrict locations, const size_t * restrict framedata,
                                size_t frameinfo_count, size_t framedata_count) {
  if ((frameinfo_count + framedata_count) > 0x80000000u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!frameinfo_count || (frameinfo_count > 1 && !framedata_count)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint64_t * indexes = ctxmalloc(context, sizeof *indexes * (frameinfo_count + framedata_count));
  for (uint_fast32_t p = 0; p < frameinfo_count; p ++)
    indexes[p] = ((uint64_t) read_be32_unaligned(context -> data + locations -> frameinfo[p]) << 32) | 0x80000000u | p;
  for (uint_fast32_t p = 0; p < framedata_count; p ++)
    indexes[p + frameinfo_count] = ((uint64_t) read_be32_unaligned(context -> data + framedata[p]) << 32) | p;
  sort_values(indexes, frameinfo_count + framedata_count);
  if (!(*indexes & 0x80000000u)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT); // fdAT before fcTL
  size_t * frames = ctxmalloc(context, sizeof *frames * frameinfo_count);
  locations -> framedata = ctxmalloc(context, sizeof *locations -> framedata * frameinfo_count);
  uint_fast32_t infoindex = 0, datacount = 0;
  // special handling for the first entry
  if (*indexes >> 32) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  *locations -> framedata = NULL;
  *frames = locations -> frameinfo[*indexes & 0x7fffffffu];
  for (uint_fast32_t p = 1; p < frameinfo_count + framedata_count; p ++) {
    if ((indexes[p] >> 32) != p) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    locations -> framedata[infoindex] = ctxrealloc(context, locations -> framedata[infoindex], sizeof **locations -> framedata * (datacount + 1));
    if (indexes[p] & 0x80000000u) {
      locations -> framedata[infoindex ++][datacount] = 0;
      locations -> framedata[infoindex] = NULL;
      frames[infoindex] = locations -> frameinfo[indexes[p] & 0x7fffffffu];
      datacount = 0;
    } else
      locations -> framedata[infoindex][datacount ++] = framedata[indexes[p] & 0x7fffffffu];
  }
  locations -> framedata[infoindex] = ctxrealloc(context, locations -> framedata[infoindex], sizeof **locations -> framedata * (datacount + 1));
  locations -> framedata[infoindex][datacount] = 0;
  memcpy(locations -> frameinfo, frames, sizeof *frames * frameinfo_count);
  ctxfree(context, frames);
  ctxfree(context, indexes);
}

uint8_t load_PNG_palette (struct context * context, const struct PNG_chunk_locations * restrict chunks, uint8_t bitdepth, uint64_t * restrict palette) {
  if (!chunks -> palette) throw(context, PLUM_ERR_UNDEFINED_PALETTE);
  uint_fast32_t count = read_be32_unaligned(context -> data + chunks -> palette - 8) / 3;
  if (count > (1 << bitdepth)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  const unsigned char * data = context -> data + chunks -> palette;
  for (uint_fast32_t p = 0; p < count; p ++) palette[p] = (data[p * 3] | ((uint64_t) data[p * 3 + 1] << 16) | ((uint64_t) data[p * 3 + 2] << 32)) * 0x101;
  if (chunks -> transparency) {
    uint_fast32_t transparency_count = read_be32_unaligned(context -> data + chunks -> transparency - 8);
    if (transparency_count > count) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    data = context -> data + chunks -> transparency;
    for (uint_fast32_t p = 0; p < transparency_count; p ++) palette[p] |= 0x101000000000000u * (0xff ^ *(data ++));
  }
  return count - 1;
}

void add_PNG_bit_depth_metadata (struct context * context, const struct PNG_chunk_locations * chunks, uint8_t imagetype, uint8_t bitdepth) {
  uint8_t red, green, blue, alpha, gray;
  switch (imagetype) {
    case 0:
      red = green = blue = 0;
      alpha = !!chunks -> transparency;
      gray = bitdepth;
      if (chunks -> bits) {
        if (read_be32_unaligned(context -> data + chunks -> bits - 8) != 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        gray = context -> data[chunks -> bits];
        if (gray > bitdepth) gray = bitdepth;
      }
      break;
    case 2:
      red = green = blue = bitdepth;
      alpha = !!chunks -> transparency;
      gray = 0;
      if (chunks -> bits) {
        if (read_be32_unaligned(context -> data + chunks -> bits - 8) != 3) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        red = context -> data[chunks -> bits];
        if (red > bitdepth) red = bitdepth;
        green = context -> data[chunks -> bits + 1];
        if (green > bitdepth) green = bitdepth;
        blue = context -> data[chunks -> bits + 2];
        if (blue > bitdepth) blue = bitdepth;
      }
      break;
    case 3:
      red = green = blue = 8;
      alpha = chunks -> transparency ? 8 : 0;
      gray = 0;
      if (chunks -> bits) {
        if (read_be32_unaligned(context -> data + chunks -> bits - 8) != 3) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        red = context -> data[chunks -> bits];
        if (red > 8) red = 8;
        green = context -> data[chunks -> bits + 1];
        if (green > 8) green = 8;
        blue = context -> data[chunks -> bits + 2];
        if (blue > 8) blue = 8;
      }
      break;
    case 4:
      red = green = blue = 0;
      gray = alpha = bitdepth;
      if (chunks -> bits) {
        if (read_be32_unaligned(context -> data + chunks -> bits - 8) != 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        gray = context -> data[chunks -> bits];
        if (gray > bitdepth) gray = bitdepth;
        alpha = context -> data[chunks -> bits + 1];
        if (alpha > bitdepth) alpha = bitdepth;
      }
      break;
    case 6:
      red = green = blue = alpha = bitdepth;
      gray = 0;
      if (chunks -> bits) {
        if (read_be32_unaligned(context -> data + chunks -> bits - 8) != 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        red = context -> data[chunks -> bits];
        if (red > bitdepth) red = bitdepth;
        green = context -> data[chunks -> bits + 1];
        if (green > bitdepth) green = bitdepth;
        blue = context -> data[chunks -> bits + 2];
        if (blue > bitdepth) blue = bitdepth;
        alpha = context -> data[chunks -> bits + 3];
        if (alpha > bitdepth) alpha = bitdepth;
      }
  }
  add_color_depth_metadata(context, red, green, blue, alpha, gray);
}

uint64_t add_PNG_background_metadata (struct context * context, const struct PNG_chunk_locations * chunks, const uint64_t * palette, uint8_t imagetype,
                                      uint8_t bitdepth, uint8_t max_palette_index, unsigned long flags) {
  if (!chunks -> background) return 0;
  uint64_t color;
  const unsigned char * data = context -> data + chunks -> background;
  switch (imagetype) {
    case 0: case 4:
      if (read_be32_unaligned(data - 8) != 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      color = read_le16_unaligned(data);
      if (color >> bitdepth) return 0;
      color = 0x100010001u * (uint64_t) bitextend16(color, bitdepth);
      break;
    case 3:
      if (read_be32_unaligned(data - 8) != 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (*data > max_palette_index) return 0;
      color = palette[*data];
      break;
    default:
      if (read_be32_unaligned(data - 8) != 6) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (bitdepth == 8) {
        if (*data || data[2] || data[4]) return 0;
        color = ((uint64_t) data[1] | ((uint64_t) data[3] << 16) | ((uint64_t) data[5] << 32)) * 0x101;
      } else
        color = read_be16_unaligned(data) | ((uint64_t) read_be16_unaligned(data + 2) << 16) | ((uint64_t) read_be16_unaligned(data + 4) << 32);
  }
  add_background_color_metadata(context, color, flags);
  return color;
}

uint64_t load_PNG_transparent_color (struct context * context, size_t offset, uint8_t imagetype, uint8_t bitdepth) {
  // only for image types 0 or 2
  const unsigned char * data = context -> data + offset;
  if (read_be32_unaligned(data - 8) != (imagetype ? 6 : 2)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!imagetype) {
    uint_fast32_t color = read_be16_unaligned(data); // cannot be 16-bit because of the potential >> 16 in the next line
    if (color >> bitdepth) return 0xffffffffffffffffu;
    return 0x100010001u * (uint64_t) bitextend16(color, bitdepth);
  } else if (bitdepth == 8) {
    if (*data || data[2] || data[4]) return 0xffffffffffffffffu;
    return ((uint64_t) data[1] | ((uint64_t) data[3] << 16) | ((uint64_t) data[5] << 32)) * 0x101;
  } else
    return (uint64_t) read_be16_unaligned(data) | ((uint64_t) read_be16_unaligned(data + 2) << 16) | ((uint64_t) read_be16_unaligned(data + 4) << 32);
}

bool check_PNG_reduced_frames (struct context * context, const struct PNG_chunk_locations * chunks) {
  for (const size_t * frameinfo = chunks -> frameinfo; *frameinfo; frameinfo ++) {
    uint_fast32_t width = read_be32_unaligned(context -> data + *frameinfo + 4);
    uint_fast32_t height = read_be32_unaligned(context -> data + *frameinfo + 8);
    uint_fast32_t left = read_be32_unaligned(context -> data + *frameinfo + 12);
    uint_fast32_t top = read_be32_unaligned(context -> data + *frameinfo + 16);
    if (top || left || width != context -> image -> width || height != context -> image -> height) return true;
  }
  return false;
}

void load_PNG_animation_frame_metadata (struct context * context, size_t offset, uint64_t * restrict duration, uint8_t * restrict disposal) {
  // returns if the previous frame should be replaced
  uint_fast16_t numerator = read_be16_unaligned(context -> data + offset + 20), denominator = read_be16_unaligned(context -> data + offset + 22);
  if ((*disposal = context -> data[offset + 24]) > 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  switch (context -> data[offset + 25]) {
    case 1:
      *disposal += PLUM_DISPOSAL_REPLACE;
    case 0:
      break;
    default:
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  if (!denominator) denominator = 100;
  *duration = numerator ? ((uint64_t) numerator * 1000000000 + denominator / 2) / denominator : 1;
}
