#include "proto.h"

struct plum_image * plum_load_image (const void * restrict buffer, size_t size, unsigned flags, unsigned * restrict error) {
  struct context context = {0};
  if (error) *error = 0;
  if (!buffer) {
    context.status = PLUM_ERR_INVALID_ARGUMENTS;
    goto errexit;
  }
  if (setjmp(context.target)) goto errexit;
  if (!(context.image = plum_new_image())) throw(&context, PLUM_ERR_OUT_OF_MEMORY);
  if (size == PLUM_FILENAME)
    load_file(&context, buffer);
  else if (size == PLUM_BUFFER) {
    context.data = ((struct plum_buffer *) buffer) -> data;
    context.size = ((struct plum_buffer *) buffer) -> size;
  } else {
    context.data = buffer;
    context.size = size;
  }
  load_image_buffer_data(&context, flags);
  if (flags & PLUM_ALPHA_REMOVE) plum_remove_alpha(context.image);
  // PLUM_PALETTE_FORCE == PLUM_PALETTE_LOAD | PLUM_PALETTE_GENERATE
  if ((flags & PLUM_PALETTE_GENERATE) && !(context.image -> palette)) {
    generate_palette(&context);
    if (!(context.image -> palette) && (flags & PLUM_PALETTE_LOAD))
      throw(&context, PLUM_ERR_TOO_MANY_COLORS);
  } else if (context.image -> palette && !(flags & PLUM_PALETTE_MASK))
    remove_palette(&context);
  destroy_allocator_list(context.allocator);
  return context.image;
  errexit:
  plum_destroy_image(context.image);
  destroy_allocator_list(context.allocator);
  if (error) *error = context.status;
  return NULL;
}

void load_image_buffer_data (struct context * context, unsigned flags) {
  if (context -> size < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (!memcmp(context -> data, (unsigned char []) {0x42, 0x4d}, 2))
    load_BMP_data(context, flags);
  else if (!memcmp(context -> data, (unsigned char []) {0x47, 0x49, 0x46, 0x38, 0x39, 0x61}, 6))
    load_GIF_data(context, flags);
  else if (!memcmp(context -> data, (unsigned char []) {0x47, 0x49, 0x46, 0x38, 0x37, 0x61}, 6))
    // treat GIF87a as GIF89a for compatibility, since it's a strict subset anyway
    load_GIF_data(context, flags);
  else
    throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
}

void load_file (struct context * context, const char * filename) {
  FILE * fp = fopen(filename, "rb");
  if (!fp) throw(context, PLUM_ERR_FILE_INACCESSIBLE);
  char * buffer = ctxmalloc(context, 0x4000 - sizeof(union allocator_node)); // keep the buffer aligned to memory pages
  size_t size = fread(buffer, 1, 0x4000 - sizeof(union allocator_node), fp);
  if (ferror(fp)) {
    fclose(fp);
    throw(context, PLUM_ERR_FILE_ERROR);
  }
  while (!feof(fp)) {
    buffer = ctxrealloc(context, buffer, size + 0x4000);
    size += fread(buffer + size, 1, 0x4000, fp);
    if (ferror(fp)) {
      fclose(fp);
      throw(context, PLUM_ERR_FILE_ERROR);
    }
  }
  fclose(fp);
  context -> data = buffer;
  context -> size = size;
}
