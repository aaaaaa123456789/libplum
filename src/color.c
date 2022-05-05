#include "proto.h"

void plum_convert_colors (void * restrict destination, const void * restrict source, size_t count, unsigned to, unsigned from) {
  if (!(source && destination && count)) return;
  if ((from & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT)) == (to & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT))) {
    memcpy(destination, source, plum_color_buffer_size(count, to));
    return;
  }
  #define convert(sp) do                                                                                       \
    if ((to & PLUM_COLOR_MASK) == PLUM_COLOR_16)                                                               \
      for (uint16_t * dp = destination; count; count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
    else if ((to & PLUM_COLOR_MASK) == PLUM_COLOR_64)                                                          \
      for (uint64_t * dp = destination; count; count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
    else                                                                                                       \
      for (uint32_t * dp = destination; count; count --) *(dp ++) = plum_convert_color(*(sp ++), from, to);    \
  while (0)
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
  // here be dragons
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
    result ^= (to & PLUM_COLOR_MASK)[(const uint64_t []) {0xff000000u, 0xffff000000000000u, 0x8000u, 0xc0000000u}];
  return result;
}

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

int image_has_transparency (const struct plum_image * image) {
  const void * colordata;
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
      const uint32_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT) {
        while (count --) if (*(color ++) < 0xff000000u) return 1;
      } else
        while (count --) if (*(color ++) >= 0x1000000u) return 1;
      return 0;
    }
    case PLUM_COLOR_64: {
      const uint64_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT) {
        while (count --) if (*(color ++) < 0xffff000000000000u) return 1;
      } else
        while (count --) if (*(color ++) >= 0x1000000000000u) return 1;
      return 0;
    }
    case PLUM_COLOR_16: {
      const uint16_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT) {
        while (count --) if (*(color ++) < 0x8000u) return 1;
      } else
        while (count --) if (*(color ++) >= 0x7fff) return 1;
      return 0;
    }
    default: { // PLUM_COLOR_32X
      const uint32_t * color = colordata;
      if (image -> color_format & PLUM_ALPHA_INVERT) {
        while (count --) if (*(color ++) < 0xc0000000u) return 1;
      } else
        while (count --) if (*(color ++) >= 0x40000000u) return 1;
      return 0;
    }
  }
}

uint32_t get_true_color_depth (const struct plum_image * image) {
  uint8_t red, green, blue, alpha;
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
    unsigned char * data = colorinfo -> data;
    if (*data || data[1] || data[2]) {
      if (*data) red = *data;
      if (data[1]) green = data[1];
      if (data[2]) blue = data[2];
    } else if (colorinfo -> size >= 5 && data[4])
      red = green = blue = data[4];
    if (colorinfo -> size >= 4 && data[3]) alpha = data[3];
  }
  if (!image_has_transparency(image)) alpha = 0;
  if (red > 16) red = 16;
  if (green > 16) green = 16;
  if (blue > 16) blue = 16;
  if (alpha > 16) alpha = 16;
  return (uint32_t) red | ((uint32_t) green << 8) | ((uint32_t) blue << 16) | ((uint32_t) alpha << 24);
}
