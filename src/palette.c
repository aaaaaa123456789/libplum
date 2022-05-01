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
  unsigned error = check_image_palette(image);
  if (error) return error;
  uint64_t sortdata[0x200];
  #define filldata(bits) do                                                                                                       \
    for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++) {                                                            \
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
  for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++) sorted[sortdata[2 * p]] = p;
  apply_sorted_palette(image, image -> color_format, sorted);
  return 0;
}

void sort_palette (struct plum_image * image, unsigned flags) {
  uint8_t indexes[0x100];
  plum_sort_colors(image -> palette, image -> max_palette_index, flags, indexes);
  uint8_t sorted[0x100];
  for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++) sorted[indexes[p]] = p;
  apply_sorted_palette(image, flags, sorted);
}

void apply_sorted_palette (struct plum_image * image, unsigned flags, const uint8_t * sorted) {
  size_t limit = (size_t) image -> width * image -> height * image -> frames;
  for (size_t p = 0; p < limit; p ++) image -> data8[p] = sorted[image -> data8[p]];
  #define sortpalette(bits) do {                                                                                         \
    uint ## bits ## _t colors[0x100];                                                                                    \
    for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++) colors[sorted[p]] = image -> palette ## bits[p];    \
    memcpy(image -> palette ## bits, colors, (image -> max_palette_index + 1) * sizeof *colors);                         \
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
  size_t size = (size_t) image -> width * image -> height * image -> frames;
  // mark all colors in the image as in use
  for (size_t p = 0; p < size; p ++) used[image -> data8[p]] = 1;
  uint64_t colors[0x200];
  // converting up to 64-bit and later back to the original format is lossless
  plum_convert_colors(colors, image -> palette, image -> max_palette_index + 1, PLUM_COLOR_64, image -> color_format);
  memcpy(colors + 0x100, colors, sizeof(uint64_t) * (image -> max_palette_index + 1));
  // expand in place from an array of colors to an interleaved array of indexes and colors; this requires looping backwards
  for (int_fast16_t p = image -> max_palette_index; p >= 0; p --) {
    colors[2 * p + 1] = colors[p];
    colors[2 * p] = p;
  }
  // sort the colors and check for duplicates; if duplicates are found, mark the duplicates as unused and the originals as in use
  qsort(colors, image -> max_palette_index + 1, 2 * sizeof *colors, &compare_index_value_pairs);
  for (uint_fast8_t p = image -> max_palette_index; p; p --) if (colors[2 * p + 1] == colors[2 * p - 1]) {
    used[colors[2 * p - 2]] |= used[colors[2 * p]];
    used[colors[2 * p]] = 0;
  }
  // create a mapping of colors (in the colors array) to indexes; colors in use (after duplicates were marked unused) get their own index
  // colors marked unused point to the previous color in use; this will deduplicate the colors, as duplicates come right after the originals
  // actually unused colors will get mapped to nonsensical indexes, but they don't matter, since they don't appear in the image
  uint_fast8_t ref = 0; // initialize it to avoid reading an uninitialized variable in the loop (even though the copied value will never be used)
  for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++)
    if (used[colors[2 * p]])
      ref = map[colors[2 * p]] = colors[2 * p];
    else
      map[colors[2 * p]] = ref;
  // update the mapping table to preserve the order of the colors in the original palette, and generate the reduced palette
  ref = 0;
  for (uint_fast16_t p = 0; p <= image -> max_palette_index; p ++)
    if (used[p]) {
      map[p] = ref;
      colors[ref ++] = colors[0x100 + p];
    } else
      map[p] = map[map[p]];
  // update the image's palette (including the max_palette_index member) and data
  image -> max_palette_index = ref - 1;
  plum_convert_colors(image -> palette, colors, ref, image -> color_format, PLUM_COLOR_64);
  for (size_t p = 0; p < size; p ++) image -> data8[p] = map[image -> data8[p]];
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
  for (const uint8_t * ptr = image -> data8; count; ptr ++, count --) if (*ptr > image -> max_palette_index) return ptr;
  return NULL;
}

int plum_get_highest_palette_index (const struct plum_image * image) {
  int result = plum_validate_image(image);
  if (result) return -result;
  if (!image -> palette) return -PLUM_ERR_UNDEFINED_PALETTE;
  // result is already initialized to 0
  size_t count = (size_t) image -> width * image -> height * image -> frames;
  for (size_t p = 0; p < count; p ++) if (image -> data8[p] > result) result = image -> data8[p];
  return result;
}

int plum_convert_colors_to_indexes (uint8_t * restrict destination, const void * restrict source, void * restrict palette, size_t count, unsigned flags) {
  if (!(destination && source && palette && count)) return -PLUM_ERR_INVALID_ARGUMENTS;
  uint64_t * colors = malloc(0x800 * sizeof *colors);
  uint64_t * sorted = malloc(0x100 * sizeof *sorted);
  uint8_t * counts = calloc(0x100, sizeof *counts);
  uint16_t * indexes = malloc(count * sizeof *indexes);
  int result = -PLUM_ERR_TOO_MANY_COLORS; // default result (which will be returned if generating the color table fails)
  if (!(colors && sorted && counts && indexes)) {
    result = -PLUM_ERR_OUT_OF_MEMORY;
    goto done;
  }
  const unsigned char * sp = source;
  unsigned total = 0, offset = plum_color_buffer_size(1, flags);
  // first, store each color in a temporary hash table, and store the index into that table for each pixel
  for (size_t pos = 0; pos < count; pos ++, sp += offset) {
    uint64_t color;
    if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
      color = *(const uint64_t *) sp;
    else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
      color = *(const uint16_t *) sp;
    else
      color = *(const uint32_t *) sp;
    uint_fast16_t index;
    unsigned char slot, hash = 0;
    for (uint_fast8_t p = 0; p < sizeof color; p ++) hash += (color >> (p * 8)) * (p + 1);
    for (slot = 0; slot < (counts[hash] & 7); slot ++) {
      index = (hash << 3) | slot;
      if (colors[index] == color) goto found;
    }
    if (slot < 7)
      counts[hash] ++; // that hash code doesn't have all seven slots occupied: use the next free one and increase the count for the hash code
    else {
      // all seven slots for that hash code are occupied: check the overflow section, and if the color is not there either, store it there
      // the hash now becomes the index into the overflow section (must be unsigned char for its overflow behavior)
      for (; counts[hash] & 0x80; hash ++) {
        index = (hash << 3) | 7; // slot == 7 here
        if (colors[index] == color) goto found;
      }
      counts[hash] |= 0x80; // mark the overflow slot for that hash code as in use
    }
    if (total >= 0x100) goto done;
    total ++;
    index = (hash << 3) | slot;
    colors[index] = color;
    found:
    indexes[pos] = index;
  }
  // then, compute a sorted color list (without gaps) to build the actual palette
  uint64_t * cc = sorted;
  for (uint_fast16_t pos = 0; pos < 0x100; pos ++) {
    uint_fast16_t index = pos << 3;
    for (uint_fast8_t p = 0; p < (counts[pos] & 7); p ++, index ++)
      *(cc ++) = (get_color_sorting_score(colors[index], flags) << 11) | index;
    if (counts[pos] & 0x80) {
      index |= 7;
      *(cc ++) = (get_color_sorting_score(colors[index], flags) << 11) | index;
    }
  }
  qsort(sorted, total, sizeof *sorted, &compare64);
  // afterwards, write the actual palette, and replace the colors with indexes into it
  #define copypalette(bits) do {                   \
    uint ## bits ## _t * pp = palette;             \
    for (size_t pos = 0; pos < total; pos ++) {    \
      *(pp ++) = colors[sorted[pos] & 0x7ff];      \
      colors[sorted[pos] & 0x7ff] = pos;           \
    }                                              \
  } while (0)
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    copypalette(64);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    copypalette(16);
  else
    copypalette(32);
  #undef copypalette
  // and finally, write out the color indexes to the frame buffer
  for (size_t pos = 0; pos < count; pos ++) destination[pos] = colors[indexes[pos]];
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
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    for (uint_fast16_t p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint64_t *) colors], flags) << 8);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    for (uint_fast16_t p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint16_t *) colors], flags) << 8);
  else
    for (uint_fast16_t p = 0; p <= max_index; p ++) keys[p] = p | (get_color_sorting_score(p[(const uint32_t *) colors], flags) << 8);
  qsort(keys, max_index + 1, sizeof *keys, &compare64);
  for (uint_fast16_t p = 0; p <= max_index; p ++) result[p] = keys[p];
}
