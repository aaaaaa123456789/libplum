#include "proto.h"

unsigned load_hierarchical_JPEG (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, double ** output) {
  unsigned component_count = get_JPEG_component_count(components);
  unsigned char componentIDs[4];
  write_le32_unaligned(componentIDs, components);
  struct JPEG_decoder_tables tables;
  initialize_JPEG_decoder_tables(context, &tables, layout);
  unsigned precision = context -> data[layout -> hierarchical + 2];
  if (precision < 2 || precision > 16) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t metadata_index = 0;
  uint16_t component_size[8] = {0}; // four widths followed by four heights
  for (size_t frame = 0; layout -> frames[frame]; frame ++) {
    if (context -> data[layout -> frames[frame] + 2] != precision) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint32_t framecomponents = determine_JPEG_components(context, layout -> frames[frame]);
    unsigned char frameIDs[4]; // IDs into the componentIDs array
    unsigned char framecount = 0;
    double * frameoutput[4] = {0};
    do {
      uint_fast8_t p;
      for (p = 0; p < component_count; p ++) if (((framecomponents >> (8 * framecount)) & 0xff) == componentIDs[p]) break;
      if (p == component_count) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      frameoutput[framecount] = output[p];
      frameIDs[framecount ++] = p;
    } while (framecount < 4 && (framecomponents >> (8 * framecount)));
    unsigned char expand = process_JPEG_metadata_until_offset(context, layout, &tables, &metadata_index, layout -> frames[frame]);
    uint16_t framewidth = read_be16_unaligned(context -> data + layout -> frames[frame] + 5);
    uint16_t frameheight = read_be16_unaligned(context -> data + layout -> frames[frame] + 3);
    if (!(framewidth && frameheight) || framewidth > context -> image -> width || frameheight > context -> image -> height)
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (layout -> frametype[frame] & 4) {
      for (uint_fast8_t p = 0; p < framecount; p ++) {
        if (!component_size[frameIDs[p]] || framewidth < component_size[frameIDs[p]] || frameheight < component_size[frameIDs[p] + 4])
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        // round all components to integers, since hierarchical progressions expect to compute differences against integers
        size_t limit = (size_t) component_size[frameIDs[p]] * component_size[frameIDs[p] + 4];
        double * data = output[frameIDs[p]];
        for (size_t index = 0; index < limit; index ++) data[index] = (uint16_t) (long) (data[index] + 65536.5); // avoid UB and round negative values correctly
      }
      if (expand) {
        double * buffer = ctxmalloc(context, sizeof *buffer * framewidth * frameheight);
        if (expand & 0x10) for (uint_fast8_t p = 0; p < framecount; p ++) {
          expand_JPEG_component_horizontally(context, output[frameIDs[p]], component_size[frameIDs[p]], component_size[frameIDs[p] + 4], framewidth, buffer);
          component_size[frameIDs[p]] = framewidth;
        }
        if (expand & 1) for (uint_fast8_t p = 0; p < framecount; p ++) {
          expand_JPEG_component_vertically(context, output[frameIDs[p]], component_size[frameIDs[p]], component_size[frameIDs[p] + 4], frameheight, buffer);
          component_size[frameIDs[p] + 4] = frameheight;
        }
        ctxfree(context, buffer);
      }
      for (uint_fast8_t p = 0; p < framecount; p ++) if (component_size[frameIDs[p]] != framewidth || component_size[frameIDs[p] + 4] != frameheight)
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    } else {
      if (expand) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      for (uint_fast8_t p = 0; p < framecount; p ++) {
        if (component_size[frameIDs[p]]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        component_size[frameIDs[p]] = framewidth;
        component_size[frameIDs[p] + 4] = frameheight;
      }
    }
    if ((layout -> frametype[frame] & 3) == 3)
      load_JPEG_lossless_frame(context, layout, framecomponents, frame, &tables, &metadata_index, frameoutput, precision, framewidth, frameheight);
    else
      load_JPEG_DCT_frame(context, layout, framecomponents, frame, &tables, &metadata_index, frameoutput, precision, framewidth, frameheight);
  }
  double normalization_offset;
  if (precision < 15)
    normalization_offset = 0.5;
  else if (precision == 15)
    normalization_offset = 0.25;
  else
    normalization_offset = 0.0;
  for (uint_fast8_t p = 0; p < component_count; p ++) {
    if (component_size[p] != context -> image -> width || component_size[p + 4] != context -> image -> height) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    normalize_JPEG_component(output[p], (size_t) context -> image -> width * context -> image -> height, normalization_offset);
  }
  return precision;
}

void expand_JPEG_component_horizontally (struct context * context, double * restrict component, size_t width, size_t height, size_t target,
                                         double * restrict buffer) {
  if ((target >> 1) > width) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t index = 0;
  for (size_t row = 0; row < height; row ++) for (size_t col = 0; col < target; col ++)
    if (col & 1)
      if (((col + 1) >> 1) == width)
        buffer[index ++] = component[(row + 1) * width - 1];
      else
        buffer[index ++] = (uint32_t) ((long) component[row * width + (col >> 1)] + (long) component[row * width + ((col + 1) >> 1)]) >> 1;
    else
      buffer[index ++] = component[row * width + (col >> 1)];
  memcpy(component, buffer, index * sizeof *component);
}

void expand_JPEG_component_vertically (struct context * context, double * restrict component, size_t width, size_t height, size_t target,
                                       double * restrict buffer) {
  if ((target >> 1) > height) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t index = 0;
  for (size_t row = 0; row < target; row ++)
    if (row & 1)
      if (((row + 1) >> 1) == height) {
        memcpy(buffer + index, component + (height - 1) * width, sizeof *component * width);
        index += width;
      } else
        for (size_t col = 0; col < width; col ++)
          buffer[index ++] = (uint32_t) ((long) component[(row >> 1) * width + col] + (long) component[((row + 1) >> 1) * width + col]) >> 1;
    else {
      memcpy(buffer + index, component + (row >> 1) * width, sizeof *component * width);
      index += width;
    }
  memcpy(component, buffer, index * sizeof *component);
}

void normalize_JPEG_component (double * restrict component, size_t count, double offset) {
  while (count --) {
    double high = *component / 65536.0 + offset;
    // this merely calculates adjustment = -floor(high); not using floor() directly to avoid linking in the math library just for a single function
    int64_t adjustment = 0;
    if (high < 0) {
      adjustment = 1 + (int64_t) -high;
      high += adjustment;
    }
    adjustment -= (int64_t) high;
    *(component ++) += adjustment * 65536.0;
  }
}
