#include "proto.h"

struct plum_image * plum_load_image (const void * restrict buffer, size_t size, unsigned flags, unsigned * restrict error) {
  return plum_load_image_limited(buffer, size, flags, SIZE_MAX, error);
}

struct plum_image * plum_load_image_limited (const void * restrict buffer, size_t size, unsigned flags, size_t limit, unsigned * restrict error) {
  struct context * context = create_context();
  if (!context) {
    if (error) *error = PLUM_ERR_OUT_OF_MEMORY;
    return NULL;
  }
  if (setjmp(context -> target)) goto done;
  if (!buffer) throw(context, PLUM_ERR_INVALID_ARGUMENTS);
  if (!(context -> image = plum_new_image())) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  prepare_image_buffer_data(context, buffer, size);
  load_image_buffer_data(context, flags, limit);
  if (flags & PLUM_ALPHA_REMOVE) plum_remove_alpha(context -> image);
  if (flags & PLUM_PALETTE_GENERATE)
    if (context -> image -> palette) {
      int colors = plum_get_highest_palette_index(context -> image);
      if (colors < 0) throw(context, -colors);
      context -> image -> max_palette_index = colors;
      update_loaded_palette(context, flags);
    } else {
      generate_palette(context, flags);
      // PLUM_PALETTE_FORCE == PLUM_PALETTE_LOAD | PLUM_PALETTE_GENERATE
      if (!(context -> image -> palette) && (flags & PLUM_PALETTE_LOAD)) throw(context, PLUM_ERR_TOO_MANY_COLORS);
    }
  else if (context -> image -> palette)
    if ((flags & PLUM_PALETTE_MASK) == PLUM_PALETTE_NONE)
      remove_palette(context);
    else
      update_loaded_palette(context, flags);
  done:
  if (error) *error = context -> status;
  struct plum_image * image = context -> image;
  if (context -> status) {
    plum_destroy_image(image);
    image = NULL;
  }
  destroy_allocator_list(context -> allocator);
  return image;
}

void load_image_buffer_data (struct context * context, unsigned flags, size_t limit) {
  if (context -> size < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (bytematch(context -> data, 0x42, 0x4d))
    load_BMP_data(context, flags, limit);
  else if (bytematch(context -> data, 0x47, 0x49, 0x46, 0x38, 0x39, 0x61))
    load_GIF_data(context, flags, limit);
  else if (bytematch(context -> data, 0x47, 0x49, 0x46, 0x38, 0x37, 0x61))
    // treat GIF87a as GIF89a for compatibility, since it's a strict subset anyway
    load_GIF_data(context, flags, limit);
  else if (bytematch(context -> data, 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a))
    // APNG files disguise as PNG files, so handle them all as PNG and split them later
    load_PNG_data(context, flags, limit);
  else if ((*context -> data == 0x50) && (context -> data[1] >= 0x31) && (context -> data[1] <= 0x37))
    load_PNM_data(context, flags, limit);
  else if (bytematch(context -> data, 0xef, 0xbb, 0xbf, 0x50) && (context -> data[4] >= 0x31) && (context -> data[4] <= 0x37))
    // text-based PNM data destroyed by a UTF-8 BOM: load it anyway, just in case a broken text editor does this
    load_PNM_data(context, flags, limit);
  else {
    // JPEG detection: one or more 0xff bytes followed by 0xd8
    size_t position;
    for (position = 0; (position < context -> size) && (context -> data[position] == 0xff); position ++);
    if (position && (position < context -> size) && (context -> data[position] == 0xd8))
      load_JPEG_data(context, flags, limit);
    else
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void prepare_image_buffer_data (struct context * context, const void * restrict buffer, size_t size) {
  switch (size) {
    case PLUM_FILENAME:
      load_file(context, buffer);
      return;
    case PLUM_BUFFER:
      context -> data = ((struct plum_buffer *) buffer) -> data;
      context -> size = ((struct plum_buffer *) buffer) -> size;
      return;
    case PLUM_CALLBACK:
      load_from_callback(context, buffer);
      return;
    default:
      context -> data = buffer;
      context -> size = size;
  }
}

void load_file (struct context * context, const char * filename) {
  FILE * fp = fopen(filename, "rb");
  if (!fp) throw(context, PLUM_ERR_FILE_INACCESSIBLE);
  size_t allocated;
  char * buffer = resize_read_buffer(context, NULL, &allocated);
  size_t size = fread(buffer, 1, allocated, fp);
  if (ferror(fp)) {
    fclose(fp);
    throw(context, PLUM_ERR_FILE_ERROR);
  }
  while (!feof(fp)) {
    if ((allocated - size) < 0x4000) buffer = resize_read_buffer(context, buffer, &allocated);
    size += fread(buffer + size, 1, 0x4000, fp);
    if (ferror(fp)) {
      fclose(fp);
      throw(context, PLUM_ERR_FILE_ERROR);
    }
  }
  fclose(fp);
  context -> data = ctxrealloc(context, buffer, size);
  context -> size = size;
}

void load_from_callback (struct context * context, const struct plum_callback * callback) {
  size_t allocated;
  char * buffer = resize_read_buffer(context, NULL, &allocated);
  int iteration = callback -> callback(callback -> userdata, buffer, 0x4000 - sizeof(union allocator_node));
  if (iteration < 0) throw(context, PLUM_ERR_FILE_ERROR);
  context -> size = iteration;
  while (iteration) {
    if ((allocated - context -> size) < 0x4000) buffer = resize_read_buffer(context, buffer, &allocated);
    iteration = callback -> callback(callback -> userdata, buffer + context -> size, 0x4000);
    if (iteration < 0) throw(context, PLUM_ERR_FILE_ERROR);
    context -> size += iteration;
  }
  context -> data = buffer;
}

void * resize_read_buffer (struct context * context, void * buffer, size_t * restrict allocated) {
  // will set the buffer to its initial size on first call (buffer = NULL, allocated = ignored), or extend it on further calls
  if (buffer)
    if (*allocated <= 0x20000u)
      *allocated += 0x4000;
    else
      *allocated += (size_t) 0x4000 << (bit_width(*allocated - 1) - 17);
  else
    *allocated = 0x4000 - sizeof(union allocator_node); // keep the buffer aligned to memory pages
  return ctxrealloc(context, buffer, *allocated);
}

void update_loaded_palette (struct context * context, unsigned flags) {
  if (flags & PLUM_SORT_EXISTING) sort_palette(context -> image, flags);
  if (flags & PLUM_PALETTE_REDUCE) {
    reduce_palette(context -> image);
    context -> image -> palette = plum_realloc(context -> image, context -> image -> palette, plum_palette_buffer_size(context -> image));
    if (!context -> image -> palette) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  }
}
