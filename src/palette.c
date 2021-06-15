#include "proto.h"

void generate_palette (struct context * context) {
  size_t count = (size_t) context -> image -> width * context -> image -> height * context -> image -> frames;
  void * palette = plum_malloc(context -> image, plum_color_buffer_size(256, context -> image -> color_format));
  uint8_t * indexes = plum_malloc(context -> image, count);
  if (!(palette || indexes)) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  int result = plum_convert_colors_to_indexes(indexes, context -> image -> data, palette, count, context -> image -> color_format);
  if (result >= 0) {
    plum_free(context -> image, context -> image -> data);
    context -> image -> data = indexes;
    context -> image -> palette = palette;
    context -> image -> max_palette_index = result;
    palette = plum_realloc(context -> image, palette, plum_color_buffer_size(result + 1, context -> image -> color_format));
    if (palette) context -> image -> palette = palette;
  } else if (result == -PLUM_ERR_TOO_MANY_COLORS) {
    plum_free(context -> image, palette);
    plum_free(context -> image, indexes);
  } else
    throw(context, -result);
}

void remove_palette (struct context * context) {
  size_t count = (size_t) context -> image -> width * context -> image -> height * context -> image -> frames;
  void * buffer = plum_malloc(context -> image, plum_color_buffer_size(count, context -> image -> color_format));
  if (!buffer) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  plum_convert_indexes_to_colors(buffer, context -> image -> data8, context -> image -> palette, count, context -> image -> color_format);
  plum_free(context -> image, context -> image -> data8);
  plum_free(context -> image, context -> image -> palette);
  context -> image -> data = buffer;
  context -> image -> palette = NULL;
  context -> image -> max_palette_index = 0;
}

void remove_alpha (struct plum_image * image) {
  void * buffer;
  size_t count;
  if (image -> palette) {
    buffer = image -> palette;
    count = image -> max_palette_index + 1;
  } else {
    buffer = image -> data;
    count = (size_t) image -> width * image -> height * image -> frames;
  }
  switch (image -> color_format & PLUM_COLOR_MASK) {
    case PLUM_COLOR_32: {
      uint32_t * p = buffer;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(p ++) |= 0xff000000u;
      else
        while (count --) *(p ++) &= 0xffffffu;
    } break;
    case PLUM_COLOR_64: {
      uint64_t * p = buffer;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(p ++) |= 0xffff000000000000u;
      else
        while (count --) *(p ++) &= 0xffffffffffffu;
    } break;
    case PLUM_COLOR_16: {
      uint16_t * p = buffer;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(p ++) |= 0x8000u;
      else
        while (count --) *(p ++) &= 0x7fffu;
    } break;
    case PLUM_COLOR_32X: {
      uint32_t * p = buffer;
      if (image -> color_format & PLUM_ALPHA_INVERT)
        while (count --) *(p ++) |= 0xc0000000u;
      else
        while (count --) *(p ++) &= 0x3fffffffu;
    }
  }
}

int plum_convert_colors_to_indexes (uint8_t * restrict destination, const void * restrict source, void * restrict palette, size_t count, unsigned flags) {
  if (!(destination && source && palette && count)) return -PLUM_ERR_INVALID_ARGUMENTS;
  uint64_t * colors = malloc(0x800 * sizeof *colors);
  uint64_t * sorted = malloc(0x100 * sizeof *sorted);
  uint8_t * counts = calloc(0x100, sizeof *counts);
  uint16_t * indexes = malloc(count * sizeof *indexes);
  int result = -PLUM_ERR_TOO_MANY_COLORS;
  if (!(colors && sorted && counts && indexes)) {
    result = -PLUM_ERR_OUT_OF_MEMORY;
    goto done;
  }
  const unsigned char * sp = source;
  unsigned p, total = 0, offset = plum_color_buffer_size(1, flags);
  uint64_t color;
  uint16_t index;
  size_t pos;
  // first, store each color in a temporary hash table, and store the index into that table for each pixel
  for (pos = 0; pos < count; pos ++, sp += offset) {
    if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
      color = *(const uint64_t *) sp;
    else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
      color = *(const uint16_t *) sp;
    else
      color = *(const uint32_t *) sp;
    unsigned char hash = 0;
    for (p = 0; p < sizeof color; p ++) hash += (color >> (p * 8)) * (p + 1);
    for (p = 0; p < (counts[hash] & 7); p ++) {
      index = (hash << 3) | p;
      if (colors[index] == color) goto found;
    }
    if (p < 7) {
      if (total >= 0x100) goto done;
      index = (hash << 3) | p;
      colors[index] = color;
      counts[hash] ++;
      total ++;
    } else {
      for (p = hash; counts[p] & 0x80; p ++) {
        index = (p << 3) | 7;
        if (colors[index] == color) goto found;
      }
      if (total >= 0x100) goto done;
      index = (p << 3) | 7;
      colors[index] = color;
      counts[hash] |= 0x80;
      total ++;
    }
    found:
    indexes[pos] = index;
  }
  // then, compute a sorted color list (without gaps) to build the actual palette
  uint64_t * cc = sorted;
  for (pos = 0; pos < 0x100; pos ++) {
    index = pos << 3;
    for (p = 0; p < (counts[pos] & 7); p ++, index ++)
      *(cc ++) = (get_color_sorting_score(colors[index], flags) << 11) | index;
    if (counts[pos] & 0x80) {
      index |= 7;
      *(cc ++) = (get_color_sorting_score(colors[index], flags) << 11) | index;
    }
  }
  qsort(sorted, total, sizeof *sorted, &compare64);
  // afterwards, write the actual palette, and replace the colors with indexes into it
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64) {
    uint64_t * pp = palette;
    for (pos = 0; pos < total; pos ++) {
      *(pp ++) = colors[sorted[pos] & 0x7ff];
      colors[sorted[pos] & 0x7ff] = pos;
    }
  } else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16) {
    uint16_t * pp = palette;
    for (pos = 0; pos < total; pos ++) {
      *(pp ++) = colors[sorted[pos] & 0x7ff];
      colors[sorted[pos] & 0x7ff] = pos;
    }
  } else {
    uint32_t * pp = palette;
    for (pos = 0; pos < total; pos ++) {
      *(pp ++) = colors[sorted[pos] & 0x7ff];
      colors[sorted[pos] & 0x7ff] = pos;
    }
  }
  // and finally, write out the color indexes to the frame buffer
  for (pos = 0; pos < count; pos ++) destination[pos] = colors[indexes[pos]];
  result = total - 1;
  done:
  free(indexes);
  free(counts);
  free(sorted);
  free(colors);
  return result;
}

uint64_t get_color_sorting_score (uint64_t color, unsigned flags) {
  color = plum_convert_color(color, flags, PLUM_COLOR_64 | PLUM_ALPHA_INVERT);
  uint64_t red = color & 0xffffu, green = (color >> 16) & 0xffffu, blue = (color >> 32) & 0xffffu, alpha = color >> 48;
  uint64_t luminance = red * 299 + green * 587 + blue * 114; // 26 bits
  uint64_t sum = red + green + blue; // 18 bits
  return ~((luminance << 27) | (sum << 9) | (alpha >> 7)); // total: 53 bits
}

int compare64 (const void * first, const void * second) {
  const uint64_t * p1 = first;
  const uint64_t * p2 = second;
  return (*p1 > *p2) - (*p1 < *p2);
}

void plum_convert_indexes_to_colors (void * restrict destination, const uint8_t * restrict source, const void * restrict palette, size_t count, unsigned flags) {
  if (!(destination && source && palette)) return;
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16) {
    uint16_t * dp = destination;
    const uint16_t * pal = palette;
    while (count --) *(dp ++) = pal[*(source ++)];
  } else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64) {
    uint64_t * dp = destination;
    const uint64_t * pal = palette;
    while (count --) *(dp ++) = pal[*(source ++)];
  } else {
    uint32_t * dp = destination;
    const uint32_t * pal = palette;
    while (count --) *(dp ++) = pal[*(source ++)];
  }
}
