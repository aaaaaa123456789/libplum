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
  switch (size) {
    case PLUM_FILENAME:
      load_file(&context, buffer);
      break;
    case PLUM_BUFFER:
      context.data = ((struct plum_buffer *) buffer) -> data;
      context.size = ((struct plum_buffer *) buffer) -> size;
      break;
    case PLUM_CALLBACK:
      load_from_callback(&context, buffer);
      break;
    default:
      context.data = buffer;
      context.size = size;
  }
  load_image_buffer_data(&context, flags);
  if (flags & PLUM_ALPHA_REMOVE) plum_remove_alpha(context.image);
  if (flags & PLUM_PALETTE_GENERATE)
    if (context.image -> palette) {
      int colors = plum_get_highest_palette_index(context.image);
      if (colors < 0) throw(&context, -colors);
      context.image -> max_palette_index = colors;
    } else {
      generate_palette(&context);
      // PLUM_PALETTE_FORCE == PLUM_PALETTE_LOAD | PLUM_PALETTE_GENERATE
      if (!(context.image -> palette) && (flags & PLUM_PALETTE_LOAD)) throw(&context, PLUM_ERR_TOO_MANY_COLORS);
    }
  else if (context.image -> palette && !(flags & PLUM_PALETTE_MASK))
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
  if (bytematch(context -> data, 0x42, 0x4d))
    load_BMP_data(context, flags);
  else if (bytematch(context -> data, 0x47, 0x49, 0x46, 0x38, 0x39, 0x61))
    load_GIF_data(context, flags);
  else if (bytematch(context -> data, 0x47, 0x49, 0x46, 0x38, 0x37, 0x61))
    // treat GIF87a as GIF89a for compatibility, since it's a strict subset anyway
    load_GIF_data(context, flags);
  else if (bytematch(context -> data, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a))
    // APNG files disguise as PNG files, so handle them all as PNG and split them later
    load_PNG_data(context, flags);
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

void load_from_callback (struct context * context, const struct plum_callback * callback) {
  char * buffer = ctxmalloc(context, 0x4000 - sizeof(union allocator_node)); // keep the buffer aligned to memory pages
  int iteration = callback -> callback(callback -> userdata, buffer, 0x4000 - sizeof(union allocator_node));
  if (iteration < 0) throw(context, PLUM_ERR_FILE_ERROR);
  context -> size = iteration;
  while (iteration) {
    buffer = ctxrealloc(context, buffer, context -> size + 0x4000);
    iteration = callback -> callback(callback -> userdata, buffer + context -> size, 0x4000);
    if (iteration < 0) throw(context, PLUM_ERR_FILE_ERROR);
    context -> size += iteration;
  }
  context -> data = buffer;
}
