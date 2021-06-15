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

void plum_convert_colors (void * restrict destination, const void * restrict source, size_t count, unsigned to, unsigned from) {
  if (!(source && destination && count)) return;
  if ((from & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT)) == (to & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT))) {
    memcpy(destination, source, plum_color_buffer_size(count, to));
    return;
  }
  #define convert(sp)                                                        \
    if ((to & PLUM_COLOR_MASK) == PLUM_COLOR_16) {                           \
      uint16_t * dp = destination;                                           \
      while (count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
    } else if ((to & PLUM_COLOR_MASK) == PLUM_COLOR_64) {                    \
      uint64_t * dp = destination;                                           \
      while (count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
    } else {                                                                 \
      uint32_t * dp = destination;                                           \
      while (count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
    }
  if ((from & PLUM_COLOR_MASK) == PLUM_COLOR_16) {
    const uint16_t * sp = source;
    convert(sp);
  } else if ((from & PLUM_COLOR_MASK) == PLUM_COLOR_64) {
    const uint64_t * sp = source;
    convert(sp);
  } else {
    const uint32_t * sp = source;
    convert(sp);
  }
  #undef convert
}

uint64_t plum_convert_color (uint64_t color, unsigned from, unsigned to) {
  uint64_t result;
  if ((from & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    from &= 0xffffu;
  else if ((from & PLUM_COLOR_MASK) != PLUM_COLOR_64)
    from &= 0xffffffffu;
  switch (((from & PLUM_COLOR_MASK) << 2) | (to & PLUM_COLOR_MASK)) {
    case 0: case 5: case 10: case 15: // no conversion
      result = color;
      break;
    case 1: // 32 to 64
      result = ((color & 0xff) | ((color << 8) & 0xff0000u) | ((color << 16) & 0xff00000000u) | ((color << 24) & 0xff000000000000u)) * 0x101;
      break;
    case 2: // 32 to 16
      result = ((color >> 3) & 0x1f) | ((color >> 6) & 0x3e0) | ((color >> 9) & 0x7c00) | ((color >> 16) & 0x8000u);
      break;
    case 3: // 32 to 32x
      result = ((color << 2) & 0x3fc) | ((color << 4) & 0xff000u) | ((color << 6) & 0x3fc00000u) | (color & 0xc0000000u) |
               ((color >> 6) & 3) | ((color >> 4) & 0xc00) | ((color >> 2) & 0x300000u);
      break;
    case 4: // 64 to 32
      result = ((color >> 8) & 0xff) | ((color >> 16) & 0xff00u) | ((color >> 24) & 0xff0000u) | ((color >> 32) & 0xff000000u);
      break;
    case 6: // 64 to 16
      result = ((color >> 11) & 0x1f) | ((color >> 22) & 0x3e0) | ((color >> 33) & 0x7c00) | ((color >> 48) & 0x8000u);
      break;
    case 7: // 64 to 32x
      result = ((color >> 6) & 0x3ff) | ((color >> 12) & 0xffc00u) | ((color >> 18) & 0x3ff00000u) | ((color >> 32) & 0xc0000000u);
      break;
    case 8: // 16 to 32
      result = ((color << 3) & 0xf8) | ((color << 6) & 0xf800u) | ((color << 9) & 0xf80000u) | ((color & 0x8000u) ? 0xff000000u : 0) |
               ((color >> 2) & 7) | ((color << 1) & 0x700) | ((color << 4) & 0x70000u);
      break;
    case 9: // 16 to 64
      result = (((color & 0x1f) | ((color << 11) & 0x1f0000u) | ((color << 22) & 0x1f00000000u)) * 0x842) | ((color & 0x8000u) ? 0xffff000000000000u : 0) |
               ((color >> 4) & 1) | ((color << 7) & 0x10000u) | ((color << 18) & 0x100000000u);
      break;
    case 11: // 16 to 32x
      result = (((color & 0x1f) | ((color << 5) & 0x7c00) | ((color << 10) & 0x1f00000u)) * 0x21) | ((color & 0x8000u) ? 0xc0000000u : 0);
      break;
    case 12: // 32x to 32
      result = ((color >> 2) & 0xff) | ((color >> 4) & 0xff00u) | ((color >> 6) & 0xff0000u) | ((color >> 30) * 0x55000000u);
      break;
    case 13: // 32x to 64
      result = ((color << 6) & 0xffc0u) | ((color << 12) & 0xffc00000u) | ((color << 18) & 0xffc000000000u) | ((color >> 30) * 0x5555000000000000u) |
               ((color >> 4) & 0x3f) | ((color << 2) & 0x3f0000u) | ((color << 8) & 0x3f00000000u);
      break;
    case 14: // 32x to 16
      result = ((color >> 5) & 0x1f) | ((color >> 10) & 0x3e0) | ((color >> 15) & 0x7c00) | ((color >> 16) & 0x8000u);
  }
  if ((to ^ from) & PLUM_ALPHA_INVERT)
    result ^= (to & PLUM_COLOR_MASK)[(uint64_t []) {0xff000000u, 0xffff000000000000u, 0x8000u, 0xc0000000u}];
  return result;
}
