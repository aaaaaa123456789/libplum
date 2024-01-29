#include "proto.h"

#define formatpair(from, to) (((from) << 2) & (PLUM_COLOR_MASK << 2) | (to) & PLUM_COLOR_MASK)

void plum_convert_colors (void * restrict destination, const void * restrict source, size_t count, unsigned long to, unsigned long from) {
  if (!(source && destination && count)) return;
  #define convert(frombits, tobits) do {                                   \
    const uint ## frombits ## _t * sp = source;                            \
    uint ## tobits ## _t * dp = destination;                               \
    while (count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
  } while (false)
  switch (formatpair(from, to)) {
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_64): convert(16, 64); break;
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_16): convert(64, 16); break;
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_32X):
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_32): convert(16, 32); break;
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_32):
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_32X): convert(64, 32); break;
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_16):
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_16): convert(32, 16); break;
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_64):
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_64): convert(32, 64); break;
    default: memcpy(destination, source, plum_color_buffer_size(count, to));
  }
  #undef convert
}

uint64_t plum_convert_color (uint64_t color, unsigned long from, unsigned long to) {
  // here be dragons
  uint64_t result;
  if ((from & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    from &= 0xffffu;
  else if ((from & PLUM_COLOR_MASK) != PLUM_COLOR_64)
    from &= 0xffffffffu;
  switch (formatpair(from, to)) {
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_32):
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_64):
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_16):
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_32X):
      result = color;
      break;
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_64):
      result = ((color & 0xff) | ((color << 8) & 0xff0000u) | ((color << 16) & 0xff00000000u) | ((color << 24) & 0xff000000000000u)) * 0x101;
      break;
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_16):
      result = ((color >> 3) & 0x1f) | ((color >> 6) & 0x3e0) | ((color >> 9) & 0x7c00) | ((color >> 16) & 0x8000u);
      break;
    case formatpair(PLUM_COLOR_32, PLUM_COLOR_32X):
      result = ((color << 2) & 0x3fc) | ((color << 4) & 0xff000u) | ((color << 6) & 0x3fc00000u) | (color & 0xc0000000u) |
               ((color >> 6) & 3) | ((color >> 4) & 0xc00) | ((color >> 2) & 0x300000u);
      break;
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_32):
      result = ((color >> 8) & 0xff) | ((color >> 16) & 0xff00u) | ((color >> 24) & 0xff0000u) | ((color >> 32) & 0xff000000u);
      break;
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_16):
      result = ((color >> 11) & 0x1f) | ((color >> 22) & 0x3e0) | ((color >> 33) & 0x7c00) | ((color >> 48) & 0x8000u);
      break;
    case formatpair(PLUM_COLOR_64, PLUM_COLOR_32X):
      result = ((color >> 6) & 0x3ff) | ((color >> 12) & 0xffc00u) | ((color >> 18) & 0x3ff00000u) | ((color >> 32) & 0xc0000000u);
      break;
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_32):
      result = ((color << 3) & 0xf8) | ((color << 6) & 0xf800u) | ((color << 9) & 0xf80000u) | ((color & 0x8000u) ? 0xff000000u : 0) |
               ((color >> 2) & 7) | ((color << 1) & 0x700) | ((color << 4) & 0x70000u);
      break;
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_64):
      result = (((color & 0x1f) | ((color << 11) & 0x1f0000u) | ((color << 22) & 0x1f00000000u)) * 0x842) | ((color & 0x8000u) ? 0xffff000000000000u : 0) |
               ((color >> 4) & 1) | ((color << 7) & 0x10000u) | ((color << 18) & 0x100000000u);
      break;
    case formatpair(PLUM_COLOR_16, PLUM_COLOR_32X):
      result = (((color & 0x1f) | ((color << 5) & 0x7c00) | ((color << 10) & 0x1f00000u)) * 0x21) | ((color & 0x8000u) ? 0xc0000000u : 0);
      break;
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_32):
      result = ((color >> 2) & 0xff) | ((color >> 4) & 0xff00u) | ((color >> 6) & 0xff0000u) | ((color >> 30) * 0x55000000u);
      break;
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_64):
      result = ((color << 6) & 0xffc0u) | ((color << 12) & 0xffc00000u) | ((color << 18) & 0xffc000000000u) | ((color >> 30) * 0x5555000000000000u) |
               ((color >> 4) & 0x3f) | ((color << 2) & 0x3f0000u) | ((color << 8) & 0x3f00000000u);
      break;
    case formatpair(PLUM_COLOR_32X, PLUM_COLOR_16):
      result = ((color >> 5) & 0x1f) | ((color >> 10) & 0x3e0) | ((color >> 15) & 0x7c00) | ((color >> 16) & 0x8000u);
  }
  if ((to ^ from) & PLUM_ALPHA_INVERT) result ^= alpha_component_masks[to & PLUM_COLOR_MASK];
  return result;
}

#undef formatpair

void plum_remove_alpha (struct plum_image * image) {
  if (!(image && image -> data && plum_check_valid_image_size(image -> width, image -> height, image -> frames))) return;
  void * colordata;
  size_t count;
  if (image -> palette) {
    colordata = image -> palette;
    count = image -> max_palette_index + 1;
  } else {
    colordata = image -> data;
    count = (size_t) image -> width * image -> height * image -> frames;
  }
  switch (image -> color_format & PLUM_COLOR_MASK) {
    case PLUM_COLOR_32: {
      uint32_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(color ++) |= 0xff000000u;
      else
        while (count --) *(color ++) &= 0xffffffu;
    } break;
    case PLUM_COLOR_64: {
      uint64_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(color ++) |= 0xffff000000000000u;
      else
        while (count --) *(color ++) &= 0xffffffffffffu;
    } break;
    case PLUM_COLOR_16: {
      uint16_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(color ++) |= 0x8000u;
      else
        while (count --) *(color ++) &= 0x7fffu;
    } break;
    case PLUM_COLOR_32X: {
      uint32_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(color ++) |= 0xc0000000u;
      else
        while (count --) *(color ++) &= 0x3fffffffu;
    }
  }
}

bool image_has_transparency (const struct plum_image * image) {
  size_t count = image -> palette ? image -> max_palette_index + 1 : ((size_t) image -> width * image -> height * image -> frames);
  #define checkcolors(bits) do {                                                                 \
    const uint ## bits ## _t * color = image -> palette ? image -> palette : image -> data;      \
    uint ## bits ## _t mask = alpha_component_masks[image -> color_format & PLUM_COLOR_MASK];    \
    if (image -> color_format & PLUM_ALPHA_INVERT) {                                             \
      while (count --) if (*(color ++) < mask) return true;                                      \
    } else {                                                                                     \
      mask = ~mask;                                                                              \
      while (count --) if (*(color ++) > mask) return true;                                      \
    }                                                                                            \
  } while (false)
  switch (image -> color_format & PLUM_COLOR_MASK) {
    case PLUM_COLOR_64: checkcolors(64); break;
    case PLUM_COLOR_16: checkcolors(16); break;
    default: checkcolors(32);
  }
  #undef checkcolors
  return false;
}

uint32_t get_color_depth (const struct plum_image * image) {
  uint_fast32_t red, green, blue, alpha;
  switch (image -> color_format & PLUM_COLOR_MASK) {
    case PLUM_COLOR_32:
      red = green = blue = alpha = 8;
      break;
    case PLUM_COLOR_64:
      red = green = blue = alpha = 16;
      break;
    case PLUM_COLOR_16:
      red = green = blue = 5;
      alpha = 1;
      break;
    case PLUM_COLOR_32X:
      red = green = blue = 10;
      alpha = 2;
  }
  const struct plum_metadata * colorinfo = plum_find_metadata(image, PLUM_METADATA_COLOR_DEPTH);
  if (colorinfo) {
    const unsigned char * data = colorinfo -> data;
    if (*data || data[1] || data[2]) {
      if (*data) red = *data;
      if (data[1]) green = data[1];
      if (data[2]) blue = data[2];
    } else if (colorinfo -> size >= 5 && data[4])
      red = green = blue = data[4];
    if (colorinfo -> size >= 4 && data[3]) alpha = data[3];
  }
  if (red > 16) red = 16;
  if (green > 16) green = 16;
  if (blue > 16) blue = 16;
  if (alpha > 16) alpha = 16;
  return red | (green << 8) | (blue << 16) | (alpha << 24);
}

uint32_t get_true_color_depth (const struct plum_image * image) {
  uint32_t result = get_color_depth(image);
  if (!image_has_transparency(image)) result &= 0xffffffu;
  return result;
}
