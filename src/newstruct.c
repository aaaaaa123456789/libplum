#include "proto.h"

struct plum_image * plum_new_image (void) {
  union allocator_node * allocator = NULL;
  struct plum_image * image = allocate(&allocator, sizeof *image);
  if (image) *image = (struct plum_image) {.allocator = allocator}; // zero-initialize all other members
  return image;
}

struct plum_image * plum_copy_image (const struct plum_image * image) {
  if (!(image && image -> data)) return NULL;
  struct plum_image * copy = plum_new_image();
  if (!copy) return NULL;
  copy -> type = image -> type;
  copy -> max_palette_index = image -> max_palette_index;
  copy -> color_format = image -> color_format;
  copy -> frames = image -> frames;
  copy -> height = image -> height;
  copy -> width = image -> width;
  copy -> userdata = image -> userdata;
  if (image -> metadata) {
    const struct plum_metadata * current = image -> metadata;
    struct plum_metadata * allocated = plum_allocate_metadata(copy, current -> size);
    if (!allocated) goto fail;
    allocated -> type = current -> type;
    memcpy(allocated -> data, current -> data, current -> size);
    struct plum_metadata * last = copy -> metadata = allocated;
    while (current = current -> next) {
      allocated = plum_allocate_metadata(copy, current -> size);
      if (!allocated) goto fail;
      allocated -> type = current -> type;
      memcpy(allocated -> data, current -> data, current -> size);
      last -> next = allocated;
      last = allocated;
    }
  }
  if (image -> width && image -> height && image -> frames) {
    size_t size = plum_pixel_buffer_size(image);
    if (!size) goto fail;
    void * buffer = plum_malloc(copy, size);
    if (!buffer) goto fail;
    memcpy(buffer, image -> data, size);
    copy -> data = buffer;
  }
  if (image -> palette) {
    size_t size = plum_palette_buffer_size(image);
    void * buffer = plum_malloc(copy, size);
    if (!buffer) goto fail;
    memcpy(buffer, image -> palette, size);
    copy -> palette = buffer;
  }
  return copy;
  fail:
  plum_destroy_image(copy);
  return NULL;
}

void plum_destroy_image (struct plum_image * image) {
  if (!image) return;
  union allocator_node * allocator = image -> allocator;
  image -> allocator = NULL;
  destroy_allocator_list(allocator);
}

struct context * create_context (void) {
  union allocator_node * allocator = NULL;
  struct context * context = allocate(&allocator, sizeof *context);
  if (context) *context = (struct context) {.allocator = allocator};
  return context;
}
