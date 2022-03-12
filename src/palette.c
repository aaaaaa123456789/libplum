#include "proto.h"

void generate_palette (struct context * context, unsigned flags) {
  size_t count = (size_t) context -> image -> width * context -> image -> height * context -> image -> frames;
  void * palette = plum_malloc(context -> image, plum_color_buffer_size(0x100, context -> image -> color_format));
  uint8_t * indexes = plum_malloc(context -> image, count);
  if (!(palette || indexes)) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  int result = plum_convert_colors_to_indexes(indexes, context -> image -> data, palette, count, flags);
  if (result >= 0) {
    plum_free(context -> image, context -> image -> data);
    context -> image -> data = indexes;
    context -> image -> max_palette_index = result;
    context -> image -> palette = plum_realloc(context -> image, palette, plum_color_buffer_size(result + 1, context -> image -> color_format));
    if (!context -> image -> palette) throw(context, PLUM_ERR_OUT_OF_MEMORY);
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

unsigned plum_sort_palette (struct plum_image * image, unsigned flags) {
  unsigned result = check_image_palette(image);
  if (!result) sort_palette(image, image -> color_format | (flags & PLUM_SORT_DARK_FIRST));
  return result;
}

unsigned plum_sort_palette_custom (struct plum_image * image, uint64_t (* callback) (void *, uint64_t), void * argument, unsigned flags) {
  if (!callback) return PLUM_ERR_INVALID_ARGUMENTS;
  unsigned p = check_image_palette(image);
  if (p) return p;
  uint64_t sortdata[0x200];
  #define filldata(bits) do                                                                                                       \
    for (p = 0; p <= image -> max_palette_index; p ++) {                                                                          \
      sortdata[2 * p] = p;                                                                                                        \
      sortdata[2 * p + 1] = callback(argument, plum_convert_color(image -> palette ## bits[p], image -> color_format, flags));    \
    }                                                                                                                             \
  while (0)
  if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    filldata(64);
  else if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    filldata(16);
  else
    filldata(32);
  #undef filldata
  qsort(sortdata, image -> max_palette_index + 1, 2 * sizeof *sortdata, &compare_index_value_pairs);
  uint8_t sorted[0x100];
  for (p = 0; p <= image -> max_palette_index; p ++) sorted[sortdata[2 * p]] = p;
  apply_sorted_palette(image, image -> color_format, sorted);
  return 0;
}

void sort_palette (struct plum_image * image, unsigned flags) {
  uint8_t indexes[0x100];
  plum_sort_colors(image -> palette, image -> max_palette_index, flags, indexes);
  uint8_t sorted[0x100];
  unsigned p;
  for (p = 0; p <= image -> max_palette_index; p ++) sorted[indexes[p]] = p;
  apply_sorted_palette(image, flags, sorted);
}

void apply_sorted_palette (struct plum_image * image, unsigned flags, const uint8_t * sorted) {
  size_t p, limit = (size_t) image -> width * image -> height * image -> frames;
  for (p = 0; p < limit; p ++) image -> data8[p] = sorted[image -> data8[p]];
  #define sortpalette(bits) do {                                                                           \
    uint ## bits ## _t colors[0x100];                                                                      \
    for (p = 0; p <= image -> max_palette_index; p ++) colors[sorted[p]] = image -> palette ## bits[p];    \
    memcpy(image -> palette ## bits, colors, p * sizeof *colors);                                          \
  } while (0)
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    sortpalette(64);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    sortpalette(16);
  else
    sortpalette(32);
  #undef sortpalette
}

unsigned plum_reduce_palette (struct plum_image * image) {
  unsigned result = check_image_palette(image);
  if (!result) reduce_palette(image);
  return result;
}

void reduce_palette (struct plum_image * image) {
  uint8_t map[0x100];
  uint8_t used[0x100] = {0};
  size_t p, ref = 0, size = (size_t) image -> width * image -> height * image -> frames;
  for (p = 0; p < size; p ++) used[image -> data8[p]] = 1;
  uint64_t colors[0x200];
  // converting up to 64-bit and later back to the original format is lossless
  plum_convert_colors(colors, image -> palette, image -> max_palette_index + 1, PLUM_COLOR_64, image -> color_format);
  memcpy(colors + 0x100, colors, sizeof(uint64_t) * (image -> max_palette_index + 1));
  for (p = image -> max_palette_index; p != SIZE_MAX; p --) {
    colors[2 * p + 1] = colors[p];
    colors[2 * p] = p;
  }
  qsort(colors, image -> max_palette_index + 1, 2 * sizeof *colors, &compare_index_value_pairs);
  for (p = image -> max_palette_index; p; p --) if (colors[2 * p + 1] == colors[2 * p - 1]) {
    used[colors[2 * p - 2]] |= used[colors[2 * p]];
    used[colors[2 * p]] = 0;
  }
  for (p = 0; p <= image -> max_palette_index; p ++)
    if (used[colors[2 * p]])
      ref = map[colors[2 * p]] = colors[2 * p];
    else
      map[colors[2 * p]] = ref;
  for (p = 0, ref = 0; p <= image -> max_palette_index; p ++)
    if (used[p]) {
      map[p] = ref;
      colors[ref ++] = colors[0x100 + p];
    } else
      map[p] = map[map[p]];
  image -> max_palette_index = ref - 1;
  plum_convert_colors(image -> palette, colors, ref, image -> color_format, PLUM_COLOR_64);
  for (p = 0; p < size; p ++) image -> data8[p] = map[image -> data8[p]];
}

unsigned check_image_palette (const struct plum_image * image) {
  unsigned result = plum_validate_image(image);
  if (result) return result;
  if (!image -> palette) return PLUM_ERR_UNDEFINED_PALETTE;
  if (plum_validate_palette_indexes(image)) return PLUM_ERR_INVALID_COLOR_INDEX;
  return 0;
}

const uint8_t * plum_validate_palette_indexes (const struct plum_image * image) {
  // NULL if OK, address of first error if failed
  if (!(image && image -> palette)) return NULL;
  if (image -> max_palette_index == 0xff) return NULL;
  size_t count = (size_t) image -> width * image -> height * image -> frames;
  const uint8_t * ptr = image -> data8;
  for (; count; ptr ++, count --) if (*ptr > image -> max_palette_index) return ptr;
  return NULL;
}

int plum_get_highest_palette_index (const struct plum_image * image) {
  int result = plum_validate_image(image);
  if (result) return -result;
  if (!image -> palette) return -PLUM_ERR_UNDEFINED_PALETTE;
  result = *(image -> data8);
  const uint8_t * data = image -> data8 + 1;
  size_t count = (size_t) image -> width * image -> height * image -> frames - 1;
  while (count --) {
    if (*data > result) result = *data;
    data ++;
  }
  return result;
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
      for (p = hash; counts[p] & 0x80; p = (p + 1) & 0xff) {
        index = (p << 3) | 7;
        if (colors[index] == color) goto found;
      }
      if (total >= 0x100) goto done;
      index = (p << 3) | 7;
      colors[index] = color;
      counts[p] |= 0x80;
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
  if (flags & PLUM_SORT_DARK_FIRST) luminance ^= 0x3ffffffu;
  uint64_t sum = red + green + blue; // 18 bits
  return ~((luminance << 27) | (sum << 9) | (alpha >> 7)); // total: 53 bits
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

void plum_sort_colors (const void * restrict colors, uint8_t max_index, unsigned flags, uint8_t * restrict result) {
  // returns the ordered color indexes
  if (!(colors && result)) return;
  uint64_t keys[0x100]; // allocate on stack to avoid dealing with malloc() failure
  unsigned p;
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    for (p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint64_t *) colors], flags) << 8);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    for (p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint16_t *) colors], flags) << 8);
  else
    for (p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint32_t *) colors], flags) << 8);
  qsort(keys, max_index + 1, sizeof *keys, &compare64);
  for (p = 0; p <= max_index; p ++) result[p] = keys[p];
}
