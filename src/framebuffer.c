#include "proto.h"

int plum_check_valid_image_size (uint32_t width, uint32_t height, uint32_t frames) {
  if (!(width && height && frames)) return 0;
  uint64_t p = width;
  const size_t limit = SIZE_MAX / sizeof(uint64_t);
  if (SIZE_MAX <= UINT32_MAX) {
    p *= height;
    if (p > limit) return 0;
    p *= frames;
    return p <= limit;
  }
  if ((p * height / height) != p) return 0;
  p *= height;
  if ((p * frames / frames) != p) return 0;
  p *= frames;
  return p <= limit;
}

size_t plum_color_buffer_size (size_t count, unsigned flags) {
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    return count * sizeof(uint64_t);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    return count * sizeof(uint16_t);
  else
    return count * sizeof(uint32_t);
}

void allocate_framebuffers (struct context * context, unsigned flags) {
  if (!plum_check_valid_image_size(context -> image -> width, context -> image -> height, context -> image -> frames))
    throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  size_t size = plum_color_buffer_size((size_t) context -> image -> width * context -> image -> height * context -> image -> frames, flags);
  if (!(context -> image -> data = plum_malloc(context -> image, size))) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  context -> image -> color_format = flags & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT);
}

void write_framebuffer_to_image (struct plum_image * image, const uint64_t * framebuffer, uint32_t frame, unsigned flags) {
  size_t framesize = plum_color_buffer_size((size_t) image -> width * image -> height, flags);
  plum_convert_colors(image -> data8 + framesize * frame, framebuffer, (size_t) image -> width * image -> height, flags, PLUM_COLOR_64);
}

void write_palette_framebuffer_to_image (struct context * context, const uint8_t * framebuffer, const uint64_t * palette, uint32_t frame, unsigned flags,
                                         uint8_t max_palette_index) {
  size_t pos, framesize = (size_t) context -> image -> width * context -> image -> height;
  if (max_palette_index < 0xff)
    for (pos = 0; pos < framesize; pos ++) if (framebuffer[pos] > max_palette_index) throw(context, PLUM_ERR_INVALID_COLOR_INDEX);
  if (context -> image -> palette) {
    memcpy(context -> image -> data8 + framesize * frame, framebuffer, framesize);
    return;
  }
  void * converted = ctxmalloc(context, (max_palette_index + 1) * sizeof(uint64_t));
  plum_convert_colors(converted, palette, max_palette_index + 1, flags, PLUM_COLOR_64);
  plum_convert_indexes_to_colors(context -> image -> data8 + plum_color_buffer_size(framesize, flags) * frame, framebuffer, converted, framesize, flags);
  ctxfree(context, converted);
}

void write_palette_to_image (struct context * context, const uint64_t * palette, unsigned flags) {
  size_t size = plum_color_buffer_size(context -> image -> max_palette_index + 1, flags);
  if (!(context -> image -> palette = plum_malloc(context -> image, size))) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  plum_convert_colors(context -> image -> palette, palette, context -> image -> max_palette_index + 1, flags, PLUM_COLOR_64);
}
