#include "proto.h"

struct plum_metadata * plum_allocate_metadata (struct plum_image * image, size_t size) {
  union {
    struct plum_metadata result;
    max_align_t alignment;
  } * result = plum_malloc(image, sizeof *result + size);
  if (result) result -> result = (struct plum_metadata) {
    .type = PLUM_METADATA_NONE,
    .size = size,
    .data = result + 1,
    .next = NULL
  };
  return &(result -> result);
}

struct plum_metadata * plum_find_metadata (const struct plum_image * image, int type) {
  struct plum_metadata * metadata;
  for (metadata = (struct plum_metadata *) image -> metadata; metadata; metadata = metadata -> next) if (metadata -> type == type) return metadata;
  return NULL;
}

void add_color_depth_metadata (struct context * context, unsigned red, unsigned green, unsigned blue, unsigned alpha, unsigned gray) {
  unsigned char counts[] = {red, green, blue, alpha, gray};
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, sizeof counts);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_COLOR_DEPTH;
  memcpy(metadata -> data, counts, sizeof counts);
  append_metadata(context -> image, metadata);
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
  append_metadata(context -> image, metadata);
}

void add_loop_count_metadata (struct context * context, uint32_t count) {
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, sizeof count);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_LOOP_COUNT;
  *(uint32_t *) (metadata -> data) = count;
  append_metadata(context -> image, metadata);
}

void add_animation_metadata (struct context * context, uint64_t ** durations, uint8_t ** disposals) {
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, sizeof **disposals * context -> image -> frames);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_FRAME_DISPOSAL;
  memset(metadata -> data, 0, metadata -> size);
  append_metadata(context -> image, metadata);
  *disposals = metadata -> data;
  metadata = plum_allocate_metadata(context -> image, sizeof **durations * context -> image -> frames);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_FRAME_DURATION;
  memset(metadata -> data, 0, metadata -> size);
  append_metadata(context -> image, metadata);
  *durations = metadata -> data;
}
