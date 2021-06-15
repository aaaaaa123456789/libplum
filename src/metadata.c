#include "proto.h"

struct plum_metadata * plum_allocate_metadata (struct plum_image * image, size_t size) {
  struct plum_metadata * result = plum_malloc(image, sizeof *result + size);
  if (result) *result = (struct plum_metadata) {
    .type = PLUM_METADATA_NONE,
    .size = size,
    .data = result + 1,
    .next = NULL
  };
  return result;
}

void add_color_depth_metadata (struct context * context, unsigned red, unsigned green, unsigned blue, unsigned alpha, unsigned gray) {
  unsigned char counts[] = {red, green, blue, alpha, gray};
  struct plum_metadata * metadata = plum_allocate_metadata(context -> image, sizeof counts);
  if (!metadata) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  metadata -> type = PLUM_METADATA_COLOR_DEPTH;
  memcpy(metadata -> data, counts, sizeof counts);
  append_metadata(context -> image, metadata);
}
