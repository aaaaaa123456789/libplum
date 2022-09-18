#include "proto.h"

struct plum_metadata * plum_allocate_metadata (struct plum_image * image, size_t size) {
  struct {
    struct plum_metadata result;
    alignas(max_align_t) unsigned char data[];
  } * result = plum_malloc(image, sizeof *result + size);
  if (result) result -> result = (struct plum_metadata) {
    .type = PLUM_METADATA_NONE,
    .size = size,
    .data = result -> data,
    .next = NULL
  };
  return (struct plum_metadata *) result;
}

unsigned plum_append_metadata (struct plum_image * image, int type, const void * data, size_t size) {
  if (!image || (size && !data)) return PLUM_ERR_INVALID_ARGUMENTS;
  struct plum_metadata * metadata = plum_allocate_metadata(image, size);
  if (!metadata) return PLUM_ERR_OUT_OF_MEMORY;
  metadata -> type = type;
  if (size) memcpy(metadata -> data, data, size);
  metadata -> next = image -> metadata;
  image -> metadata = metadata;
  return 0;
}

struct plum_metadata * plum_find_metadata (const struct plum_image * image, int type) {
  if (!image) return NULL;
  for (struct plum_metadata * metadata = (struct plum_metadata *) image -> metadata; metadata; metadata = metadata -> next)
    if (metadata -> type == type) return metadata;
  return NULL;
}

void add_color_depth_metadata (struct context * context, unsigned red, unsigned green, unsigned blue, unsigned alpha, unsigned gray) {
  unsigned char counts[] = {red, green, blue, alpha, gray};
  unsigned result = plum_append_metadata(context -> image, PLUM_METADATA_COLOR_DEPTH, counts, sizeof counts);
  if (result) throw(context, result);
}

void add_background_color_metadata (struct context * context, uint64_t color, unsigned flags) {
  color = plum_convert_color(color, PLUM_COLOR_64, flags);
  size_t size = plum_color_buffer_size(1, flags);
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, size);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_BACKGROUND;
  if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    *(uint64_t *) (metadata -> data) = color;
  else if ((flags & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    *(uint16_t *) (metadata -> data) = color;
  else
    *(uint32_t *) (metadata -> data) = color;
  metadata -> next = context -> image -> metadata;
  context -> image -> metadata = metadata;
}

void add_loop_count_metadata (struct context * context, uint32_t count) {
  unsigned result = plum_append_metadata(context -> image, PLUM_METADATA_LOOP_COUNT, &count, sizeof count);
  if (result) throw(context, result);
}

void add_animation_metadata (struct context * context, uint64_t ** restrict durations, uint8_t ** restrict disposals) {
  struct plum_metadata * durations_metadata = plum_allocate_metadata(context -> image, sizeof **durations * context -> image -> frames);
  struct plum_metadata * disposals_metadata = plum_allocate_metadata(context -> image, sizeof **disposals * context -> image -> frames);
  if (!(durations_metadata && disposals_metadata)) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  memset(*durations = durations_metadata -> data, 0, durations_metadata -> size);
  memset(*disposals = disposals_metadata -> data, 0, disposals_metadata -> size);
  durations_metadata -> type = PLUM_METADATA_FRAME_DURATION;
  disposals_metadata -> type = PLUM_METADATA_FRAME_DISPOSAL;
  durations_metadata -> next = disposals_metadata;
  disposals_metadata -> next = context -> image -> metadata;
  context -> image -> metadata = durations_metadata;
}

struct plum_rectangle * add_frame_area_metadata (struct context * context) {
  if (context -> image -> frames > SIZE_MAX / sizeof(struct plum_rectangle)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, sizeof(struct plum_rectangle) * context -> image -> frames);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_FRAME_AREA;
  metadata -> next = context -> image -> metadata;
  context -> image -> metadata = metadata;
  return metadata -> data;
}

uint64_t get_background_color (const struct plum_image * image, uint64_t fallback) {
  const struct plum_metadata * background = plum_find_metadata(image, PLUM_METADATA_BACKGROUND);
  if (!background) return fallback;
  if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_64)
    return *(const uint64_t *) background -> data;
  else if ((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_16)
    return *(const uint16_t *) background -> data;
  else
    return *(const uint32_t *) background -> data;
}
