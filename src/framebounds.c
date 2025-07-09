#include "proto.h"

struct plum_rectangle * get_frame_boundaries (struct context * context, bool anchor_corner) {
  const struct plum_metadata * metadata = plum_find_metadata(context -> source, PLUM_METADATA_FRAME_AREA);
  if (!metadata) return NULL;
  struct plum_rectangle * result = ctxmalloc(context, sizeof *result * context -> source -> frames);
  uint_fast32_t frames = (context -> source -> frames > metadata -> size / sizeof *result) ? metadata -> size / sizeof *result : context -> source -> frames;
  if (frames) {
    memcpy(result, metadata -> data, sizeof *result * frames);
    if (anchor_corner)
      for (uint_fast32_t frame = 0; frame < frames; frame ++) {
        result[frame].width += result[frame].left;
        result[frame].height += result[frame].top;
        result[frame].top = result[frame].left = 0;
      }
  }
  for (uint_fast32_t frame = frames; frame < context -> source -> frames; frame ++)
    result[frame] = (struct plum_rectangle) {.left = 0, .top = 0, .width = context -> source -> width, .height = context -> source -> height};
  return result;
}

void adjust_frame_boundaries (const struct plum_image * image, struct plum_rectangle * restrict boundaries) {
  uint64_t empty_color = get_empty_color(image);
  #define checkframe(type) do                                                                                                                             \
    for (uint_fast32_t frame = 0; frame < image -> frames; frame ++) {                                                                                    \
      for (size_t remaining = (size_t) boundaries[frame].top * image -> width; remaining; remaining --)                                                   \
        if (notempty(type)) goto done ## type;                                                                                                            \
      if (boundaries[frame].left || boundaries[frame].width != image -> width)                                                                            \
        for (uint_fast32_t row = 0; row < boundaries[frame].height; row ++) {                                                                             \
          for (uint_fast32_t col = 0; col < boundaries[frame].left; col ++) if (notempty(type)) goto done ## type;                                        \
          index += boundaries[frame].width;                                                                                                               \
          for (uint_fast32_t col = boundaries[frame].left + boundaries[frame].width; col < image -> width; col ++)                                        \
            if (notempty(type)) goto done ## type;                                                                                                        \
        }                                                                                                                                                 \
      else                                                                                                                                                \
        index += (size_t) boundaries[frame].height * image -> width;                                                                                      \
      for (size_t remaining = (size_t) (image -> height - boundaries[frame].top - boundaries[frame].height) * image -> width; remaining; remaining --)    \
        if (notempty(type)) goto done ## type;                                                                                                            \
      continue;                                                                                                                                           \
      done ## type:                                                                                                                                       \
      boundaries[frame] = (struct plum_rectangle) {.left = 0, .top = 0, .width = image -> width, .height = image -> height};                              \
    }                                                                                                                                                     \
  while (false)
  if (image -> palette) {
    bool empty[256];
    switch (image -> color_format & PLUM_COLOR_MASK) {
      case PLUM_COLOR_64:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) empty[p] = image -> palette64[p] == empty_color;
        break;
      case PLUM_COLOR_16:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) empty[p] = image -> palette16[p] == empty_color;
        break;
      default:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) empty[p] = image -> palette32[p] == empty_color;
    }
    size_t index = 0;
    #define notempty(...) (!empty[image -> data8[index ++]])
    checkframe(pal);
  } else {
    size_t index = 0;
    #undef notempty
    #define notempty(bits) (image -> data ## bits[index ++] != empty_color)
    switch (image -> color_format & PLUM_COLOR_MASK) {
      case PLUM_COLOR_16: checkframe(16); break;
      case PLUM_COLOR_64: checkframe(64); break;
      default: checkframe(32);
    }
  }
  #undef notempty
  #undef checkframe
}

bool image_rectangles_have_transparency (const struct plum_image * image, const struct plum_rectangle * rectangles) {
  size_t framesize = (size_t) image -> width * image -> height;
  if (image -> palette) {
    bool transparent[256];
    uint_fast64_t mask = alpha_component_masks[image -> color_format & PLUM_COLOR_MASK], match = (image -> color_format & PLUM_ALPHA_INVERT) ? mask : 0;
    switch (image -> color_format & PLUM_COLOR_MASK) {
      case PLUM_COLOR_64:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) transparent[p] = (image -> palette64[p] & mask) != match;
        break;
      case PLUM_COLOR_16:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) transparent[p] = (image -> palette16[p] & mask) != match;
        break;
      default:
        for (size_t p = 0; p <= image -> max_palette_index; p ++) transparent[p] = (image -> palette32[p] & mask) != match;
    }
    for (uint_fast32_t frame = 0; frame < image -> frames; frame ++) for (uint_fast32_t row = 0; row < rectangles[frame].height; row ++) {
      const uint8_t * rowdata = image -> data8 + framesize * frame + (size_t) image -> width * (row + rectangles[frame].top) + rectangles[frame].left;
      for (uint_fast32_t col = 0; col < rectangles[frame].width; col ++) if (transparent[rowdata[col]]) return true;
    }
  } else {
    #define checkcolors(bits, address, count) do {                                                 \
      const uint ## bits ## _t * color = (const void *) address;                                   \
      size_t remaining = count;                                                                    \
      uint ## bits ## _t mask = alpha_component_masks[image -> color_format & PLUM_COLOR_MASK];    \
      if (image -> color_format & PLUM_ALPHA_INVERT) {                                             \
        while (remaining --) if (*(color ++) < mask) return true;                                  \
      } else {                                                                                     \
        mask = ~mask;                                                                              \
        while (remaining --) if (*(color ++) > mask) return true;                                  \
      }                                                                                            \
    } while (false)
    for (uint_fast32_t frame = 0; frame < image -> frames; frame ++) {
      size_t frameoffset = framesize * frame + (size_t) rectangles[frame].top * image -> width + rectangles[frame].left;
      const uint8_t * framedata = image -> data8 + plum_color_buffer_size(frameoffset, image -> color_format);
      if (rectangles[frame].left || rectangles[frame].width != image -> width) {
        size_t rowsize = plum_color_buffer_size(image -> width, image -> color_format);
        for (uint_fast32_t row = 0; row < rectangles[frame].height; row ++, framedata += rowsize)
          switch (image -> color_format & PLUM_COLOR_MASK) {
            case PLUM_COLOR_64: checkcolors(64, framedata, rectangles[frame].width); break;
            case PLUM_COLOR_16: checkcolors(16, framedata, rectangles[frame].width); break;
            default: checkcolors(32, framedata, rectangles[frame].width);
          }
      } else {
        size_t count = (size_t) rectangles[frame].height * image -> width;
        switch (image -> color_format & PLUM_COLOR_MASK) {
          case PLUM_COLOR_64: checkcolors(64, framedata, count); break;
          case PLUM_COLOR_16: checkcolors(16, framedata, count); break;
          default: checkcolors(32, framedata, count);
        }
      }
    }
    #undef checkcolors
  }
  return false;
}
