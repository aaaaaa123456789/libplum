#include "proto.h"

int plum_check_valid_image_size (uint32_t width, uint32_t height, uint32_t frames) {
  if (!(width && height && frames)) return 0;
  size_t p = width;
  const size_t limit = SIZE_MAX / sizeof(uint64_t);
  if ((p * height / height) != p) return 0;
  p *= height;
  if ((p * frames / frames) != p) return 0;
  p *= frames;
  return p <= limit;
}

size_t plum_color_buffer_size (size_t count, unsigned flags) {
  if (count > (SIZE_MAX / sizeof(uint64_t))) return 0;
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    return count * sizeof(uint64_t);
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    return count * sizeof(uint16_t);
  else
    return count * sizeof(uint32_t);
}

size_t plum_pixel_buffer_size (const struct plum_image * image) {
  if (!image) return 0;
  if (!plum_check_valid_image_size(image -> width, image -> height, image -> frames)) return 0;
  size_t count = (size_t) image -> width * image -> height * image -> frames;
  if (!image -> palette) count = plum_color_buffer_size(count, image -> color_format);
  return count;
}

size_t plum_palette_buffer_size (const struct plum_image * image) {
  if (!image) return 0;
  return plum_color_buffer_size(image -> max_palette_index + 1, image -> color_format);
}

void allocate_framebuffers (struct context * context, unsigned flags, int palette) {
  if (!plum_check_valid_image_size(context -> image -> width, context -> image -> height, context -> image -> frames))
    throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  size_t size = (size_t) context -> image -> width * context -> image -> height * context -> image -> frames;
  if (!palette) size = plum_color_buffer_size(size, flags);
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

unsigned plum_rotate_image (struct plum_image * image, unsigned count, int flip) {
  unsigned error = plum_validate_image(image);
  if (error) return error;
  count &= 3;
  if (!(count || flip)) return 0;
  size_t framesize = (size_t) image -> width * image -> height;
  void * buffer;
  if (image -> palette)
    buffer = malloc(framesize);
  else
    buffer = malloc(plum_color_buffer_size(framesize, image -> color_format));
  if (!buffer) return PLUM_ERR_OUT_OF_MEMORY;
  if (count & 1) {
    uint_fast32_t temp = image -> width;
    image -> width = image -> height;
    image -> height = temp;
  }
  size_t (* coordinate) (size_t, size_t, size_t, size_t);
  switch (count) {
    case 0: coordinate = flip_coordinate; break; // we know flip has to be enabled because null rotations were excluded already
    case 1: coordinate = flip ? rotate_right_flip_coordinate : rotate_right_coordinate; break;
    case 2: coordinate = flip ? rotate_both_flip_coordinate : rotate_both_coordinate; break;
    case 3: coordinate = flip ? rotate_left_flip_coordinate : rotate_left_coordinate;
  }
  uint_fast32_t frame;
  if (image -> palette)
    for (frame = 0; frame < image -> frames; frame ++) rotate_frame_8(image -> data8 + framesize * frame, buffer, image -> width, image -> height, coordinate);
  else if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    for (frame = 0; frame < image -> frames; frame ++) rotate_frame_64(image -> data64 + framesize * frame, buffer, image -> width, image -> height, coordinate);
  else if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    for (frame = 0; frame < image -> frames; frame ++) rotate_frame_16(image -> data16 + framesize * frame, buffer, image -> width, image -> height, coordinate);
  else
    for (frame = 0; frame < image -> frames; frame ++) rotate_frame_32(image -> data32 + framesize * frame, buffer, image -> width, image -> height, coordinate);
  free(buffer);
  return 0;
}

#define ROTATE_FRAME_FUNCTION(bits) \
void rotate_frame_ ## bits (uint ## bits ## _t * restrict frame, uint ## bits ## _t * restrict buffer, size_t width, size_t height, \
                            size_t (* coordinate) (size_t, size_t, size_t, size_t)) {                                               \
  size_t row, col;                                                                                                                  \
  for (row = 0; row < height; row ++) for (col = 0; col < width; col ++)                                                            \
    buffer[row * width + col] = frame[coordinate(row, col, width, height)];                                                         \
  memcpy(frame, buffer, sizeof *frame * width * height);                                                                            \
}

ROTATE_FRAME_FUNCTION(8)
ROTATE_FRAME_FUNCTION(16)
ROTATE_FRAME_FUNCTION(32)
ROTATE_FRAME_FUNCTION(64)

#undef ROTATE_FRAME_FUNCTION

size_t rotate_left_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return (col + 1) * height - (row + 1);
}

size_t rotate_right_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return (width - 1 - col) * height + row;
}

size_t rotate_both_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return height * width - 1 - (row * width + col);
}

size_t flip_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return (height - 1 - row) * width + col;
}

size_t rotate_left_flip_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return col * height + row;
}

size_t rotate_right_flip_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return height * width - 1 - (col * height + row);
}

size_t rotate_both_flip_coordinate (size_t row, size_t col, size_t width, size_t height) {
  return (row + 1) * width - (col + 1);
}
