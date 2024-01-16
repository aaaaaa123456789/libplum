#include "proto.h"

size_t plum_store_image (const struct plum_image * image, void * restrict buffer, size_t size_mode, unsigned * restrict error) {
  struct context * context = create_context();
  if (!context) {
    if (error) *error = PLUM_ERR_OUT_OF_MEMORY;
    return 0;
  }
  context -> source = image;
  if (!setjmp(context -> target)) {
    if (!(image && buffer && size_mode)) throw(context, PLUM_ERR_INVALID_ARGUMENTS);
    unsigned rv = plum_validate_image(image);
    if (rv) throw(context, rv);
    if (plum_validate_palette_indexes(image)) throw(context, PLUM_ERR_INVALID_COLOR_INDEX);
    switch (image -> type) {
      case PLUM_IMAGE_BMP: generate_BMP_data(context); break;
      case PLUM_IMAGE_GIF: generate_GIF_data(context); break;
      case PLUM_IMAGE_PNG: generate_PNG_data(context); break;
      case PLUM_IMAGE_APNG: generate_APNG_data(context); break;
      case PLUM_IMAGE_JPEG: generate_JPEG_data(context); break;
      case PLUM_IMAGE_PNM: generate_PNM_data(context); break;
      case PLUM_IMAGE_QOI: generate_QOI_data(context); break;
      default: throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    size_t output_size = get_total_output_size(context);
    if (!output_size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    switch (size_mode) {
      case PLUM_MODE_FILENAME:
        write_generated_image_data_to_file(context, buffer);
        break;
      case PLUM_MODE_BUFFER: {
        void * out = malloc(output_size);
        if (!out) throw(context, PLUM_ERR_OUT_OF_MEMORY);
        // the function must succeed after reaching this point (otherwise, memory would be leaked)
        *(struct plum_buffer *) buffer = (struct plum_buffer) {.size = output_size, .data = out};
        write_generated_image_data(out, context -> output);
      } break;
      case PLUM_MODE_CALLBACK:
        write_generated_image_data_to_callback(context, buffer);
        break;
      default:
        if (output_size > size_mode) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
        write_generated_image_data(buffer, context -> output);
    }
    context -> size = output_size;
  }
  if (context -> file) fclose(context -> file);
  if (error) *error = context -> status;
  size_t result = context -> size;
  destroy_allocator_list(context -> allocator);
  return result;
}

void write_generated_image_data_to_file (struct context * context, const char * filename) {
  context -> file = fopen(filename, "wb");
  if (!context -> file) throw(context, PLUM_ERR_FILE_INACCESSIBLE);
  const struct data_node * node;
  for (node = context -> output; node -> previous; node = node -> previous);
  while (node) {
    const unsigned char * data = node -> data;
    size_t size = node -> size;
    while (size) {
      unsigned count = fwrite(data, 1, (size > 0x4000) ? 0x4000 : size, context -> file);
      if (ferror(context -> file) || !count) throw(context, PLUM_ERR_FILE_ERROR);
      data += count;
      size -= count;
    }
    node = node -> next;
  }
  fclose(context -> file);
  context -> file = NULL;
}

void write_generated_image_data_to_callback (struct context * context, const struct plum_callback * callback) {
  struct data_node * node;
  for (node = context -> output; node -> previous; node = node -> previous);
  while (node) {
    unsigned char * data = node -> data; // not const because the callback takes an unsigned char *
    size_t size = node -> size;
    while (size) {
      int block = (size > 0x4000) ? 0x4000 : size;
      int count = callback -> callback(callback -> userdata, data, block);
      if (count < 0 || count > block) throw(context, PLUM_ERR_FILE_ERROR);
      data += count;
      size -= count;
    }
    node = node -> next;
  }
}

void write_generated_image_data (void * restrict buffer, const struct data_node * data) {
  const struct data_node * node;
  for (node = data; node -> previous; node = node -> previous);
  for (unsigned char * out = buffer; node; node = node -> next) {
    memcpy(out, node -> data, node -> size);
    out += node -> size;
  }
}

size_t get_total_output_size (struct context * context) {
  size_t result = 0;
  for (const struct data_node * node = context -> output; node; node = node -> previous) {
    if (result + node -> size < result) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
    result += node -> size;
  }
  return result;
}
