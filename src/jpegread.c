#include "proto.h"

void load_JPEG_data (struct context * context, unsigned long flags, size_t limit) {
  struct JPEG_marker_layout * layout = load_JPEG_marker_layout(context); // will be leaked (to be collected by context release)
  uint32_t components = determine_JPEG_components(context, layout -> hierarchical ? layout -> hierarchical : *layout -> frames);
  void (* transfer) (uint64_t * restrict, size_t, unsigned, const double **) = get_JPEG_component_transfer_function(context, layout, components);
  context -> image -> type = PLUM_IMAGE_JPEG;
  context -> image -> frames = 1;
  if (layout -> hierarchical) {
    context -> image -> width = read_be16_unaligned(context -> data + layout -> hierarchical + 5);
    context -> image -> height = read_be16_unaligned(context -> data + layout -> hierarchical + 3);
  } else {
    if (layout -> frames[1]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    context -> image -> width = read_be16_unaligned(context -> data + *layout -> frames + 5);
    context -> image -> height = read_be16_unaligned(context -> data + *layout -> frames + 3);
    for (size_t p = 0; layout -> markers[p]; p ++) if (layout -> markertype[p] == 0xdc) { // DNL marker
      if (read_be16_unaligned(context -> data + layout -> markers[p]) != 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      uint_fast16_t markerheight = read_be16_unaligned(context -> data + layout -> markers[p] + 2);
      if (!markerheight) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      if (!context -> image -> height)
        context -> image -> height = markerheight;
      else if (context -> image -> height != markerheight)
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
  }
  validate_image_size(context, limit);
  size_t count = (size_t) context -> image -> width * context -> image -> height;
  double * component_data[4] = {0};
  for (uint_fast8_t p = 0; p < get_JPEG_component_count(components); p ++) component_data[p] = ctxmalloc(context, sizeof **component_data * count);
  unsigned bitdepth;
  if (layout -> hierarchical)
    bitdepth = load_hierarchical_JPEG(context, layout, components, component_data);
  else
    bitdepth = load_single_frame_JPEG(context, layout, components, component_data);
  append_JPEG_color_depth_metadata(context, transfer, bitdepth);
  allocate_framebuffers(context, flags, false);
  unsigned maxvalue = ((uint32_t) 1 << bitdepth) - 1;
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64) {
    transfer(context -> image -> data64, count, maxvalue, (const double **) component_data);
    if (flags & PLUM_ALPHA_INVERT) for (size_t p = 0; p < count; p ++) context -> image -> data64[p] ^= 0xffff000000000000u;
  } else {
    uint64_t * buffer = ctxmalloc(context, count * sizeof *buffer);
    transfer(buffer, count, maxvalue, (const double **) component_data);
    plum_convert_colors(context -> image -> data, buffer, count, flags, PLUM_COLOR_64);
    ctxfree(context, buffer);
  }
  for (uint_fast8_t p = 0; p < 4; p ++) ctxfree(context, component_data[p]); // unused components will be NULL anyway
  if (layout -> Exif) {
    unsigned rotation = get_JPEG_rotation(context, layout -> Exif);
    if (rotation) {
      unsigned error = plum_rotate_image(context -> image, rotation & 3, rotation & 4);
      if (error) throw(context, error);
    }
  }
}

struct JPEG_marker_layout * load_JPEG_marker_layout (struct context * context) {
  size_t offset = 1;
  while (context -> data[offset ++] == 0xff); // the first marker must be SOI (from file type detection), so skip it
  uint_fast8_t next_restart_marker = 0; // 0 if not in a scan
  size_t restart_offset, restart_interval, scan, frame = SIZE_MAX, markers = 0;
  struct JPEG_marker_layout * layout = ctxmalloc(context, sizeof *layout);
  *layout = (struct JPEG_marker_layout) {0}; // ensure that integers and pointers are properly zero-initialized
  while (offset < context -> size) {
    size_t prev = offset;
    if (context -> data[offset ++] != 0xff)
      if (next_restart_marker)
        continue;
      else
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    while (offset < context -> size && context -> data[offset] == 0xff) offset ++;
    if (offset >= context -> size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint_fast8_t marker = context -> data[offset ++];
    if (!marker)
      if (next_restart_marker)
        continue;
      else
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (marker < 0xc0 || marker == 0xc8 || marker == 0xd8 || (marker >= 0xf0 && marker != 0xfe)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (next_restart_marker) {
      layout -> framedata[frame][scan] = ctxrealloc(context, layout -> framedata[frame][scan], sizeof ***layout -> framedata * (restart_interval + 2));
      layout -> framedata[frame][scan][restart_interval ++] = restart_offset;
      layout -> framedata[frame][scan][restart_interval ++] = prev - restart_offset;
    }
    if (marker == 0xd9) break;
    if (marker == next_restart_marker) {
      if (++ next_restart_marker == 0xd8) next_restart_marker = 0xd0;
      restart_offset = offset;
      continue;
    } else if ((marker & ~7u) == 0xd0)
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    // if we find a marker other than RST, we're definitely ending the current scan, and the marker definitely has a size
    if (next_restart_marker) {
      layout -> framedata[frame][scan] = ctxrealloc(context, layout -> framedata[frame][scan], sizeof ***layout -> framedata * (restart_interval + 1));
      layout -> framedata[frame][scan][restart_interval] = 0;
      next_restart_marker = 0;
    }
    if (offset > context -> size - 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint_fast16_t marker_size = read_be16_unaligned(context -> data + offset);
    if (marker_size < 2 || marker_size > context -> size || offset > context -> size - marker_size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    switch (marker) {
      case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc5: case 0xc6:
      case 0xc7: case 0xc9: case 0xca: case 0xcb: case 0xcd: case 0xce: case 0xcf:
        // start a new frame
        if (frame != SIZE_MAX) {
          if (scan == SIZE_MAX) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          layout -> framescans[frame] = ctxrealloc(context, layout -> framescans[frame], sizeof **layout -> framescans * ((++ scan) + 1));
          layout -> framescans[frame][scan] = 0;
        }
        layout -> frames = ctxrealloc(context, layout -> frames, sizeof *layout -> frames * ((size_t) (++ frame) + 1));
        layout -> frames[frame] = offset;
        layout -> framescans = ctxrealloc(context, layout -> framescans, sizeof *layout -> framescans * (frame + 1));
        layout -> framescans[frame] = NULL;
        layout -> framedata = ctxrealloc(context, layout -> framedata, sizeof *layout -> framedata * (frame + 1));
        layout -> framedata[frame] = NULL;
        layout -> frametype = ctxrealloc(context, layout -> frametype, sizeof *layout -> frametype * (frame + 1));
        layout -> frametype[frame] = marker & 15;
        scan = SIZE_MAX;
        break;
      case 0xda:
        // start a new scan
        if (frame == SIZE_MAX) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        layout -> framescans[frame] = ctxrealloc(context, layout -> framescans[frame], sizeof **layout -> framescans * ((size_t) (++ scan) + 1));
        layout -> framescans[frame][scan] = offset;
        layout -> framedata[frame] = ctxrealloc(context, layout -> framedata[frame], sizeof **layout -> framedata * (scan + 1));
        layout -> framedata[frame][scan] = NULL;
        restart_interval = 0;
        restart_offset = offset + marker_size;
        next_restart_marker = 0xd0;
        break;
      case 0xde:
        if (layout -> hierarchical || frame != SIZE_MAX) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        layout -> hierarchical = offset;
        break;
      case 0xdf:
        if (!layout -> hierarchical) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      case 0xc4: case 0xcc: case 0xdb: case 0xdc: case 0xdd:
        layout -> markers = ctxrealloc(context, layout -> markers, sizeof *layout -> markers * (markers + 1));
        layout -> markers[markers] = offset;
        layout -> markertype = ctxrealloc(context, layout -> markertype, sizeof *layout -> markertype * (markers + 1));
        layout -> markertype[markers ++] = marker;
        break;
      // For JFIF, Exif and Adobe markers, all want to come "first", i.e., immediately after SOI. This is obviously impossible if more than one is present.
      // Therefore, "first" is interpreted to mean "before any SOF/DHP marker" here.
      case 0xe0:
        if (layout -> JFIF || layout -> hierarchical || frame != SIZE_MAX) break;
        if (marker_size >= 7 && bytematch(context -> data + offset + 2, 0x4a, 0x46, 0x49, 0x46, 0x00)) layout -> JFIF = offset;
        break;
      case 0xe1:
        if (layout -> Exif || layout -> hierarchical || frame != SIZE_MAX) break;
        if (marker_size >= 16 && bytematch(context -> data + offset + 2, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00)) layout -> Exif = offset;
        break;
      case 0xee:
        if (layout -> Adobe || layout -> hierarchical || frame != SIZE_MAX) break;
        if (marker_size >= 9 && bytematch(context -> data + offset + 2, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x00) &&
            (context -> data[offset + 8] == 100 || context -> data[offset + 8] == 101))
          layout -> Adobe = offset;
    }
    offset += marker_size;
  }
  if (frame == SIZE_MAX) throw(context, PLUM_ERR_NO_DATA);
  if (scan == SIZE_MAX) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  layout -> markers = ctxrealloc(context, layout -> markers, sizeof *layout -> markers * (markers + 1));
  layout -> markers[markers] = 0;
  if (next_restart_marker) {
    layout -> framedata[frame][scan] = ctxrealloc(context, layout -> framedata[frame][scan], sizeof ***layout -> framedata * (restart_interval + 1));
    layout -> framedata[frame][scan][restart_interval] = 0;
  }
  layout -> framescans[frame] = ctxrealloc(context, layout -> framescans[frame], sizeof **layout -> framescans * (++ scan + 1));
  layout -> framescans[frame][scan] = 0;
  layout -> frames = ctxrealloc(context, layout -> frames, sizeof *layout -> frames * (++ frame + 1));
  layout -> frames[frame] = 0;
  return layout;
}

unsigned get_JPEG_rotation (struct context * context, size_t offset) {
  // returns rotation count in bits 0-1 and vertical inversion in bit 2
  uint_fast16_t size = read_be16_unaligned(context -> data + offset);
  const unsigned char * data = context -> data + offset + 8;
  size -= 8;
  uint_fast16_t tag = read_le16_unaligned(data);
  bool bigendian;
  if (tag == 0x4949)
    bigendian = false; // little endian
  else if (tag == 0x4d4d)
    bigendian = true;
  else
    return 0;
  tag = bigendian ? read_be16_unaligned(data + 2) : read_le16_unaligned(data + 2);
  if (tag != 42) return 0;
  uint_fast32_t pos = bigendian ? read_be32_unaligned(data + 4) : read_le32_unaligned(data + 4);
  if (pos > size - 2) return 0;
  uint_fast16_t count = bigendian ? read_be16_unaligned(data + pos) : read_le16_unaligned(data + pos);
  pos += 2;
  if (size - pos < (uint_fast32_t) count * 12) return 0;
  for (; count; pos += 12, count --) {
    tag = bigendian ? read_be16_unaligned(data + pos) : read_le16_unaligned(data + pos);
    if (tag == 0x112) break; // 0x112 = orientation data
  }
  if (!count) return 0;
  tag = bigendian ? read_be16_unaligned(data + pos + 2) : read_le16_unaligned(data + pos + 2);
  uint_fast32_t datasize = bigendian ? read_be32_unaligned(data + pos + 4) : read_le32_unaligned(data + pos + 4);
  if (tag != 3 || datasize != 1) return 0;
  tag = bigendian ? read_be16_unaligned(data + pos + 8) : read_le16_unaligned(data + pos + 8);
  static const unsigned rotations[] = {0, 6, 2, 4, 7, 1, 5, 3};
  if (-- tag >= sizeof rotations / sizeof *rotations) tag = 0;
  return rotations[tag];
}

unsigned load_single_frame_JPEG (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components, double ** output) {
  if (*layout -> frametype & 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  struct JPEG_decoder_tables tables;
  initialize_JPEG_decoder_tables(context, &tables, layout);
  unsigned precision = context -> data[*layout -> frames + 2];
  if (precision < 2 || precision > 16) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  size_t metadata_index = 0;
  if (*layout -> frametype == 3 || *layout -> frametype == 11)
    load_JPEG_lossless_frame(context, layout, components, 0, &tables, &metadata_index, output, precision, context -> image -> width, context -> image -> height);
  else
    load_JPEG_DCT_frame(context, layout, components, 0, &tables, &metadata_index, output, precision, context -> image -> width, context -> image -> height);
  return precision;
}

unsigned char process_JPEG_metadata_until_offset (struct context * context, const struct JPEG_marker_layout * layout, struct JPEG_decoder_tables * tables,
                                                  size_t * restrict index, size_t limit) {
  unsigned char expansion = 0;
  for (; layout -> markers[*index] && layout -> markers[*index] < limit; ++ *index) {
    const unsigned char * markerdata = context -> data + layout -> markers[*index];
    uint16_t markersize = read_be16_unaligned(markerdata) - 2;
    markerdata += 2;
    switch (layout -> markertype[*index]) {
      case 0xc4: // DHT
        while (markersize) {
          if (*markerdata & ~0x13u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          unsigned char target = (*markerdata & 3) | (*markerdata >> 2);
          markerdata ++;
          markersize --;
          if (tables -> Huffman[target]) ctxfree(context, tables -> Huffman[target]);
          tables -> Huffman[target] = process_JPEG_Huffman_table(context, &markerdata, &markersize);
        }
        break;
      case 0xcc: // DAC
        if (markersize % 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        for (uint_fast16_t count = markersize / 2; count; count --) {
          unsigned char target = *(markerdata ++);
          if (target & ~0x13u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          target = (target >> 2) | (target & 3);
          if (target & 4) {
            if (!*markerdata || *markerdata > 63) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          } else
            if ((*markerdata >> 4) < (*markerdata & 15)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          tables -> arithmetic[target] = *(markerdata ++);
        }
        break;
      case 0xdb: // DQT
        while (markersize) {
          if (*markerdata & ~0x13u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          unsigned char target = *markerdata & 3, type = *markerdata >> 4, length = type ? 128 : 64;
          markerdata ++;
          if (-- markersize < length) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
          markersize -= length;
          if (!tables -> quantization[target]) tables -> quantization[target] = ctxmalloc(context, 64 * sizeof *tables -> quantization[target]);
          if (type)
            for (uint_fast8_t p = 0; p < 64; p ++, markerdata += 2) tables -> quantization[target][p] = read_be16_unaligned(markerdata);
          else
            for (uint_fast8_t p = 0; p < 64; p ++) tables -> quantization[target][p] = *(markerdata ++);
        }
        break;
      case 0xdd: // DRI
        if (markersize != 2) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        tables -> restart = read_be16_unaligned(markerdata);
        break;
      case 0xdf: // EXP
        if (markersize != 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        expansion = *markerdata;
    }
  }
  return expansion;
}
