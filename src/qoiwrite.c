#include "proto.h"

#define equalpixels(p1, p2) ((p1).r == (p2).r && (p1).g == (p2).g && (p1).b == (p2).b && (p1).a == (p2).a)

void generate_QOI_data (struct context * context) {
  if (context -> source -> frames > 1) throw(context, PLUM_ERR_NO_MULTI_FRAME);
  if (!(context -> source -> width && context -> source -> height)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  unsigned char * header = append_output_node(context, 14);
  bytewrite(header, 0x71, 0x6f, 0x69, 0x66);
  write_be32_unaligned(header + 4, context -> source -> width);
  write_be32_unaligned(header + 8, context -> source -> height);
  uint8_t channels = 3 + image_has_transparency(context -> source);
  bytewrite(header + 12, channels, 0);
  size_t framesize = (size_t) context -> source -> width * context -> source -> height;
  uint32_t * data;
  if ((context -> source -> color_format & (PLUM_COLOR_MASK | PLUM_ALPHA_INVERT)) == (PLUM_COLOR_32 | PLUM_ALPHA_INVERT))
    data = context -> source -> data;
  else {
    data = ctxmalloc(context, sizeof *data * framesize);
    plum_convert_colors(data, context -> source -> data, framesize, PLUM_COLOR_32 | PLUM_ALPHA_INVERT, context -> source -> color_format);
  }
  unsigned char * node = append_output_node(context, framesize * (channels + 1) + 8);
  unsigned char * output = node;
  struct QOI_pixel lookup[64] = {0};
  struct QOI_pixel prev = {.r = 0, .g = 0, .b = 0, .a = 0xff};
  uint8_t run = 0;
  for (size_t cell = 0; cell < framesize; cell ++) {
    struct QOI_pixel px = {.r = data[cell], .g = data[cell] >> 8, .b = data[cell] >> 16, .a = data[cell] >> 24};
    if (equalpixels(px, prev)) {
      run ++;
      if (run == 62 || cell == framesize - 1) {
        *(output ++) = 0xc0 | (run - 1);
        run = 0;
      }
    } else {
      if (run) {
        *(output ++) = 0xc0 | (run - 1);
        run = 0;
      }
      uint8_t index = (px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11) % (sizeof lookup / sizeof *lookup);
      if (equalpixels(px, lookup[index]))
        *(output ++) = index;
      else {
        lookup[index] = px;
        if (px.a == prev.a) {
          int8_t dr = px.r - prev.r, dg = px.g - prev.g, db = px.b - prev.b;
          int8_t drg = dr - dg, dbg = db - dg;
          if (dr >= -2 && dr < 2 && dg >= -2 && dg < 2 && db >= -2 && db < 2)
            *(output ++) = 0x40 | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
          else if (drg >= -8 && drg < 8 && dg >= -32 && dg < 32 && dbg >= -8 && dbg < 8)
            output += byteappend(output, 0x80 | (dg + 32), ((drg + 8) << 4) | (dbg + 8));
          else
            output += byteappend(output, 0xfe, px.r, px.g, px.b);
        } else
          output += byteappend(output, 0xff, px.r, px.g, px.b, px.a);
      }
      prev = px;
    }
  }
  output += byteappend(output, 0, 0, 0, 0, 0, 0, 0, 1);
  resize_output_node(context, node, output - node);
  if (data != context -> source -> data) ctxfree(context, data);
}

#undef equalpixels
