#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#define PLUM_UNPREFIXED_MACROS
#include "libplum.h"

#define bytematch(address, ...) (!memcmp((address), (unsigned char []) {__VA_ARGS__}, sizeof (unsigned char []) {__VA_ARGS__}))
#define bytewrite(address, ...) (memcpy(address, (unsigned char []) {__VA_ARGS__}, sizeof (unsigned char []) {__VA_ARGS__}))
#define byteappend(address, ...) (bytewrite(address, __VA_ARGS__), sizeof (unsigned char []) {__VA_ARGS__})

static void process_loaded_image(struct plum_image *);
static void test_image(struct plum_image *);
static struct plum_image * load_image_transformed(const struct plum_buffer *, uint16_t);
static void generate_animation_metadata(struct plum_image *);
static struct plum_buffer identity_transform(const unsigned char *, size_t);
static struct plum_buffer GIF_image_data_transform(const unsigned char *, size_t);
static struct plum_buffer sanitize_PNG_transform(const unsigned char *, size_t);
static struct plum_buffer fixed_PNG_image_data_transform(const unsigned char *, size_t);
static struct plum_buffer generic_PNG_image_data_transform(const unsigned char *, size_t);
static struct plum_image * regular_image_generator(const unsigned char *, size_t);
static struct plum_image * palette_image_generator(const unsigned char *, size_t);
static uint64_t sort_colors(void *, uint64_t);
static uint32_t compute_PNG_CRC(const unsigned char *, size_t);

struct plum_buffer (* transforms[]) (const unsigned char *, size_t) = {
  identity_transform,
  GIF_image_data_transform,
  sanitize_PNG_transform,
  fixed_PNG_image_data_transform,
  generic_PNG_image_data_transform
};

// generators should include a plum_buffer with unused input data in the image's user member
struct plum_image * (* generators[]) (const unsigned char *, size_t) = {
  regular_image_generator,
  palette_image_generator
};

static inline uint16_t read_le16_unaligned (const unsigned char * data) {
  return (uint16_t) *data | ((uint16_t) data[1] << 8);
}

static inline uint32_t read_be32_unaligned (const unsigned char * data) {
  return (uint32_t) data[3] | ((uint32_t) data[2] << 8) | ((uint32_t) data[1] << 16) | ((uint32_t) *data << 24);
}

static inline void write_be32_unaligned (unsigned char * buffer, uint32_t value) {
  *(buffer ++) = value >> 24;
  *(buffer ++) = value >> 16;
  *(buffer ++) = value >> 8;
  *buffer = value;
}

const char * __asan_default_options (void) {
  return "detect_invalid_pointer_pairs=2:allocator_may_return_null=1";
}

int LLVMFuzzerTestOneInput (const unsigned char * data, size_t size) {
  unsigned transform;
  struct plum_image * image;
  for (transform = 0; transform < (sizeof transforms / sizeof *transforms); transform ++) {
    struct plum_buffer buffer = transforms[transform](data, size);
    struct plum_image * image = plum_load_image_limited(&buffer, PLUM_MODE_BUFFER, PLUM_COLOR_64 | PLUM_PALETTE_LOAD, 0x1000000, NULL);
    free(buffer.data);
    if (image) process_loaded_image(image);
    plum_destroy_image(image);
    if (size >= 2) {
      uint16_t flags = read_le16_unaligned(data + size - 2);
      buffer = transforms[transform](data, size - 2);
      image = load_image_transformed(&buffer, flags);
      free(buffer.data);
      if (image) process_loaded_image(image);
      plum_destroy_image(image);
    }
  }
  for (transform = 0; transform < (sizeof generators / sizeof *generators); transform ++) {
    image = generators[transform](data, size);
    if (image) test_image(image);
    plum_destroy_image(image);
  }
  return 0;
}

static void process_loaded_image (struct plum_image * image) {
  struct plum_buffer buffer;
  unsigned format;
  for (format = 1; format < PLUM_NUM_IMAGE_TYPES; format ++) {
    image -> type = format;
    if (plum_store_image(image, &buffer, PLUM_MODE_BUFFER, NULL)) plum_free(NULL, buffer.data); // free(buffer.data), but testing plum_free
  }
}

static struct plum_image * load_image_transformed (const struct plum_buffer * buffer, uint16_t flags) {
  struct plum_image * image = plum_load_image_limited(buffer, PLUM_MODE_BUFFER, flags & 0x3f07, 0x1000000, NULL);
  if (!image) return NULL;
  plum_rotate_image(image, flags >> 14, flags & 8);
  unsigned newformat = (flags >> 8) & 7;
  if (newformat != image -> color_format) {
    size_t count;
    void ** original;
    void * converted;
    if (image -> palette) {
      original = &image -> palette;
      count = image -> max_palette_index + 1;
    } else {
      original = &image -> data;
      count = (size_t) image -> width * image -> height * image -> frames;
    }
    converted = plum_malloc(image, plum_color_buffer_size(count, newformat));
    if (converted) {
      plum_convert_colors(converted, *original, count, newformat, image -> color_format);
      plum_free(image, *original);
      *original = converted;
      struct plum_metadata * metadata = plum_find_metadata(image, PLUM_METADATA_BACKGROUND);
      if (metadata) {
        unsigned char colorbuffer[sizeof(uint64_t)];
        plum_convert_colors(colorbuffer, metadata -> data, 1, newformat, image -> color_format);
        metadata -> type = PLUM_METADATA_NONE;
        plum_append_metadata(image, PLUM_METADATA_BACKGROUND, colorbuffer, plum_color_buffer_size(1, newformat));
      }
      image -> color_format = newformat;
    }
  }
  plum_sort_palette_custom(image, &sort_colors, NULL, (flags >> 4) & 7);
  return image;
}

static void test_image (struct plum_image * image) {
  process_loaded_image(image);
  struct plum_image * copy = plum_copy_image(image);
  plum_remove_alpha(copy);
  process_loaded_image(copy);
  plum_destroy_image(copy);
  size_t count = (size_t) image -> width * image -> height * image -> frames;
  if (image -> palette) {
    plum_reduce_palette(image);
    plum_sort_palette(image, 0);
    void * palette = image -> palette;
    image -> palette = NULL;
    void * newdata = plum_calloc(image, plum_pixel_buffer_size(image));
    plum_convert_indexes_to_colors(newdata, image -> data8, palette, count, image -> color_format);
    plum_free(image, palette);
    plum_free(image, image -> data8);
    image -> data = newdata;
  } else {
    void * palette = plum_calloc(image, plum_color_buffer_size(256, image -> color_format));
    uint8_t * newdata = plum_calloc(image, count);
    int result = plum_convert_colors_to_indexes(newdata, image -> data, palette, count, image -> color_format);
    if (result < 0) {
      plum_free(image, palette);
      plum_free(image, newdata);
    } else {
      plum_free(image, image -> data);
      image -> data8 = newdata;
      image -> palette = palette;
      image -> max_palette_index = result;
    }
  }
  unsigned frames;
  for (frames = 7; frames > 2; frames --) if (!(image -> height % frames)) {
    image -> height /= frames;
    image -> frames *= frames;
    break;
  }
  generate_animation_metadata(image);
  process_loaded_image(image);
}

static void generate_animation_metadata (struct plum_image * image) {
  struct plum_buffer * buffer = image -> userdata;
  if (!buffer) return;
  if (buffer -> size >= sizeof(uint32_t)) {
    plum_append_metadata(image, PLUM_METADATA_LOOP_COUNT, buffer -> data, sizeof(uint32_t));
    buffer -> data = (unsigned char *) buffer -> data + sizeof(uint32_t);
    buffer -> size -= sizeof(uint32_t);
  }
  if (!buffer -> size) return;
  size_t frame, frames = buffer -> size * 2;
  if (frames > image -> frames) frames = image -> frames;
  struct plum_metadata * metadata = plum_allocate_metadata(image, frames);
  metadata -> next = image -> metadata;
  metadata -> type = PLUM_METADATA_FRAME_DISPOSAL;
  image -> metadata = metadata;
  for (frame = 0; frame < frames; frame ++)
    frame[(unsigned char *) metadata -> data] = (((const unsigned char *) buffer -> data)[frame >> 1] >> (4 * (frame & 1))) % PLUM_NUM_DISPOSAL_METHODS;
  buffer -> data = (unsigned char *) buffer -> data + (frames + 1) / 2;
  buffer -> size -= (frames + 1) / 2;
  frames = buffer -> size / sizeof(uint64_t);
  if (!frames) return;
  if (frames > image -> frames) frames = image -> frames;
  plum_append_metadata(image, PLUM_METADATA_FRAME_DURATION, buffer -> data, sizeof(uint64_t) * frames);
  buffer -> data = (unsigned char *) buffer -> data + sizeof(uint64_t) * frames;
  buffer -> size -= sizeof(uint64_t) * frames;
  if (!buffer -> size) return;
  frames = buffer -> size / sizeof(struct plum_rectangle);
  if (frames > image -> frames) frames = image -> frames;
  plum_append_metadata(image, PLUM_METADATA_FRAME_AREA, buffer -> data, sizeof(struct plum_rectangle) * frames);
  buffer -> data = (unsigned char *) buffer -> data + sizeof(struct plum_rectangle) * frames;
  buffer -> size -= sizeof(struct plum_rectangle) * frames;
}

static struct plum_buffer identity_transform (const unsigned char * data, size_t size) {
  struct plum_buffer result = {.size = size, .data = malloc(size + !size)};
  memcpy(result.data, data, size);
  return result;
}

static struct plum_buffer GIF_image_data_transform (const unsigned char * data, size_t size) {
  if (size < 7) return (struct plum_buffer) {.size = 0, .data = NULL};
  struct plum_buffer result;
  unsigned char * current = result.data = malloc(size + (size / 255) + 888);
  current += byteappend(current, 0x47, 0x49, 0x46, 0x38, 0x39, 0x61, data[1], data[6] >> 6, data[2], (data[6] & 0x30) >> 4,
                                (*data & 0x77) | 0x80, 0, 0);
  unsigned index;
  for (index = 0; index < (2 << (*data & 7)); index ++) current += byteappend(current, index, index, index);
  current += byteappend(current, 0x21, 0xf9, 0x04, *data >> 7, data[3], data[4], data[5], 0,
                                 0x2c, 0, 0, 0, 0, data[1], data[6] >> 6, data[2], (data[6] & 0x30) >> 4, (*data & 8) << 3);
  *(current ++) = data[6] & 15;
  data += 7;
  size -= 7;
  while (size >= 0xff) {
    *(current ++) = 0xff;
    memcpy(current, data, 0xff);
    current += 0xff;
    data += 0xff;
    size -= 0xff;
  }
  if (size) {
    *(current ++) = size;
    memcpy(current, data, size);
    current += size;
  }
  current += byteappend(current, 0, 0x3b);
  result.size = current - (unsigned char *) result.data;
  return result;
}

static struct plum_buffer sanitize_PNG_transform (const unsigned char * data, size_t size) {
  if (size < 20) return (struct plum_buffer) {.size = 0, .data = NULL};
  struct plum_buffer result = {.size = size, .data = malloc(size + 12)};
  unsigned char * output = result.data;
  memcpy(output, data, size);
  size_t offset = byteappend(output, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a);
  while (offset < size) {
    if ((size - offset) < 12) break;
    uint32_t chunklength = read_be32_unaligned(output + offset);
    size_t limit = size - offset - 12;
    if (limit > 0x7fffffffu) limit = 0x7fffffffu;
    if (chunklength > limit) {
      chunklength = limit;
      write_be32_unaligned(output + offset, chunklength);
    }
    offset += 4;
    uint32_t chunktype = read_be32_unaligned(output + offset), check = compute_PNG_CRC(output + offset, chunklength + 4);
    offset += chunklength + 4;
    write_be32_unaligned(output + offset, check);
    offset += 4;
    if (chunktype == 0x49454e44) {
      result.size = offset;
      return result;
    }
  }
  bytewrite(output + offset, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82);
  result.size = offset + 12;
  return result;
}

static struct plum_buffer fixed_PNG_image_data_transform (const unsigned char * data, size_t size) {
  const unsigned char header[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x01, 0x50, 0x00, 0x00, 0x01, 0x50, 0x08, 0x06, 0x00, 0x00, 0x00, 0xe9, 0xe8, 0x26, 0xd9
  };
  const unsigned char footer[] = {0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
  struct plum_buffer result = {.size = size + sizeof header + sizeof footer + 12};
  unsigned char * current = result.data = malloc(result.size);
  memcpy(current, header, sizeof header);
  current += sizeof header;
  write_be32_unaligned(current, size);
  current += 4;
  write_be32_unaligned(current, 0x49444154);
  current += 4;
  memcpy(current, data, size);
  current += size;
  write_be32_unaligned(current, compute_PNG_CRC(current - (size + 4), size + 4));
  current += 4;
  memcpy(current, footer, sizeof footer);
  return result;
}

static struct plum_buffer generic_PNG_image_data_transform (const unsigned char * data, size_t size) {
  if ((size < 3) || ((size == 3) && !(*data & 15))) return (struct plum_buffer) {.size = 0, .data = NULL};
  struct plum_buffer result = {.data = malloc(size + 68)};
  unsigned char * current = result.data;
  current += byteappend(current, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d);
  unsigned char type = *data & 15;
  unsigned char bitwidth = type[(const unsigned char []) {8, 1, 2, 4, 8, 16, 8, 16, 1, 2, 4, 8, 8, 16, 8, 16}];
  type = type[(const unsigned char []) {3, 0, 0, 0, 0, 0, 2, 2, 3, 3, 3, 3, 4, 4, 6, 6}];
  size_t block = byteappend(current, 0x49, 0x48, 0x44, 0x52, 0, 0, *data >> 6, data[1], 0, 0, (*data & 0x10) >> 4, data[2],
                                     bitwidth, type, 0, 0, (*data & 0x20) >> 5);
  write_be32_unaligned(current + block, compute_PNG_CRC(current, block));
  current += block + 4;
  unsigned colorcount = (type == 3) ? 1 << bitwidth : 0;
  if (!(*data & 15)) {
    colorcount = 1 + data[3];
    data += 4;
    size -= 4;
  } else {
    data += 3;
    size -= 3;
  }
  colorcount *= 3;
  if (colorcount) {
    if (size < colorcount) {
      free(result.data);
      return (struct plum_buffer) {.size = 0, .data = NULL};
    }
    current += byteappend(current, 0, 0, colorcount >> 8, colorcount, 0x50, 0x4c, 0x54, 0x45);
    memcpy(current, data, colorcount);
    write_be32_unaligned(current + colorcount, compute_PNG_CRC(current - 4, colorcount + 4));
    current += colorcount + 4;
    data += colorcount;
    size -= colorcount;
  }
  write_be32_unaligned(current, size + 2);
  current += 4;
  bytewrite(current, 0x49, 0x44, 0x41, 0x54, 0x78, 0x5e);
  memcpy(current + 6, data, size);
  write_be32_unaligned(current + size + 6, compute_PNG_CRC(current, size + 6));
  current += size + 10;
  current += byteappend(current, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82);
  result.size = current - (unsigned char *) result.data;
  return result;
}

static struct plum_image * regular_image_generator (const unsigned char * data, size_t size) {
  if (!size) return NULL;
  unsigned color_format = *data & 7;
  uint32_t height = (*data >> 4) * 17 + ((*data & 8) ? 8 : 1);
  data ++;
  size --;
  size_t rowsize = plum_color_buffer_size(height, color_format);
  if (size < rowsize) return NULL;
  struct plum_image * image = plum_new_image();
  if (!image) return NULL;
  image -> color_format = color_format;
  image -> height = height;
  image -> width = size / rowsize;
  image -> frames = 1;
  size_t imagesize = plum_pixel_buffer_size(image);
  image -> data = plum_malloc(image, imagesize);
  if (!image -> data) {
    plum_destroy_image(image);
    return NULL;
  }
  memcpy(image -> data, data, imagesize);
  data += imagesize;
  size -= imagesize;
  if (size >= 2) {
    uint16_t depth = read_le16_unaligned(data);
    const unsigned char colordepth[] = {(depth & 15) + 1, ((depth >> 4) & 15) + 1, ((depth >> 8) & 15) + 1, ((depth >> 12) & 15) + 1};
    plum_append_metadata(image, PLUM_METADATA_COLOR_DEPTH, colordepth, sizeof colordepth);
    data += 2;
    size -= 2;
  }
  image -> userdata = plum_calloc(image, sizeof(struct plum_buffer));
  *(struct plum_buffer *) image -> userdata = (struct plum_buffer) {.data = (unsigned char *) data, .size = size};
  return image;
}

static struct plum_image * palette_image_generator (const unsigned char * data, size_t size) {
  if (size < 2) return NULL;
  unsigned color_format = *data & 7, palcount = data[1] + 1;
  uint32_t height = (*data >> 4) * 17 + ((*data & 8) ? 8 : 1);
  size_t index, palsize = plum_color_buffer_size(palcount, color_format);
  if (size < (2 + palsize + height)) return NULL;
  struct plum_image * image = plum_new_image();
  if (!image) return NULL;
  image -> color_format = color_format;
  image -> height = height;
  image -> frames = 1;
  image -> palette = plum_malloc(image, palsize);
  image -> max_palette_index = data[1];
  if (!image -> palette) {
    plum_destroy_image(image);
    return NULL;
  }
  memcpy(image -> palette, data + 2, palsize);
  data += palsize + 2;
  size -= palsize + 2;
  image -> width = size / height;
  size_t imagesize = (size_t) height * image -> width;
  image -> data = plum_calloc(image, imagesize);
  if (!image -> data) {
    plum_destroy_image(image);
    return NULL;
  }
  for (index = 0; index < imagesize; index ++) image -> data8[index] = data[index] % palcount;
  data += imagesize;
  size -= imagesize;
  if (size >= plum_color_buffer_size(1, color_format)) {
    plum_append_metadata(image, PLUM_METADATA_BACKGROUND, data, plum_color_buffer_size(1, color_format));
    data += plum_color_buffer_size(1, color_format);
    size -= plum_color_buffer_size(1, color_format);
  }
  plum_append_metadata(image, PLUM_METADATA_COLOR_DEPTH, (unsigned char []) {0, 0, 0, 0}, 4);
  image -> userdata = plum_calloc(image, sizeof(struct plum_buffer));
  *(struct plum_buffer *) image -> userdata = (struct plum_buffer) {.data = (unsigned char *) data, .size = size};
  return image;
}

static uint64_t sort_colors (void * unused, uint64_t color) {
  (void) unused;
  return color;
}

static uint32_t compute_PNG_CRC (const unsigned char * data, size_t size) {
  static const uint32_t table[] = {
    /* 0x00 */ 0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    /* 0x08 */ 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    /* 0x10 */ 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    /* 0x18 */ 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    /* 0x20 */ 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    /* 0x28 */ 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    /* 0x30 */ 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    /* 0x38 */ 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    /* 0x40 */ 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    /* 0x48 */ 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    /* 0x50 */ 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    /* 0x58 */ 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    /* 0x60 */ 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    /* 0x68 */ 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    /* 0x70 */ 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    /* 0x78 */ 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    /* 0x80 */ 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    /* 0x88 */ 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    /* 0x90 */ 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    /* 0x98 */ 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    /* 0xa0 */ 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    /* 0xa8 */ 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    /* 0xb0 */ 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    /* 0xb8 */ 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    /* 0xc0 */ 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    /* 0xc8 */ 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    /* 0xd0 */ 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    /* 0xd8 */ 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    /* 0xe0 */ 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    /* 0xe8 */ 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    /* 0xf0 */ 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    /* 0xf8 */ 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
  };
  uint32_t checksum = 0xffffffff;
  while (size --) checksum = (checksum >> 8) ^ table[(uint8_t) checksum ^ *(data ++)];
  return ~checksum;
}
