#include "proto.h"

#define pixelcolor(px) (((uint32_t) (px).a << 24) | ((uint32_t) (px).b << 16) | ((uint32_t) (px).g << 8) | (uint32_t) (px).r)

void load_QOI_data (struct context * context, unsigned flags, size_t limit) {
  if (context -> size < 22) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  context -> image -> type = PLUM_IMAGE_QOI;
  context -> image -> frames = 1;
  context -> image -> width = read_be32_unaligned(context -> data + 4);
  context -> image -> height = read_be32_unaligned(context -> data + 8);
  validate_image_size(context, limit);
  allocate_framebuffers(context, flags, false);
  add_color_depth_metadata(context, 8, 8, 8, 8, 0);
  size_t framesize = (size_t) context -> image -> width * context -> image -> height;
  uint32_t * frame = ctxmalloc(context, sizeof *frame * framesize);
  const unsigned char * data = context -> data + 14;
  const unsigned char * dataend = context -> data + context -> size;
  struct QOI_pixel lookup[64] = {0};
  struct QOI_pixel px = {.r = 0, .g = 0, .b = 0, .a = 0xff};
  for (size_t cell = 0; cell < framesize; cell ++) {
    if (data + 1 < dataend) {
      unsigned char v = *(data ++);
      if (v == 0xfe && data + 3 < dataend) {
        px.r = *(data ++);
        px.g = *(data ++);
        px.b = *(data ++);
      } else if (v == 0xff && data + 4 < dataend) {
        px.r = *(data ++);
        px.g = *(data ++);
        px.b = *(data ++);
        px.a = *(data ++);
      } else if (!(v & 0xc0) && v < sizeof lookup / sizeof *lookup)
        px = lookup[v];
      else if ((v & 0xc0) == 0x40) {
        px.r += ((v >> 4) & 3) - 2;
        px.g += ((v >> 2) & 3) - 2;
        px.b += (v & 3) - 2;
      } else if ((v & 0xc0) == 0x80 && data + 1 < dataend) {
        unsigned char v2 = *(data ++);
        int8_t dg = (v & 0x3f) - 32;
        px.r += dg + ((v2 >> 4) & 0xf) - 8;
        px.g += dg;
        px.b += dg + (v2 & 0xf) - 8;
      } else if ((v & 0xc0) == 0xc0) {
        uint8_t run = v & 0x3f;
        if (cell + run >= framesize) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        uint32_t color = pixelcolor(px);
        while (run --) frame[cell ++] = color;
      } else
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      lookup[(px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11) % (sizeof lookup / sizeof *lookup)] = px;
    }
    frame[cell] = pixelcolor(px);
  }
  plum_convert_colors(context -> image -> data8, frame, framesize, flags, PLUM_COLOR_32 | PLUM_ALPHA_INVERT);
  ctxfree(context, frame);
}

#undef pixelcolor
