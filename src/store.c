#include "proto.h"

size_t plum_store_image (const struct plum_image * image, void * restrict buffer, size_t size, unsigned * restrict error) {
  struct context context = {0};
  context.source = image;
  if (setjmp(context.target)) goto done;
  if (!(image && buffer && size)) throw(&context, PLUM_ERR_INVALID_ARGUMENTS);
  if (!(image -> width && image -> height && image -> frames)) throw(&context, PLUM_ERR_NO_DATA);
  if (!plum_check_valid_image_size(image -> width, image -> height, image -> frames)) throw(&context, PLUM_ERR_IMAGE_TOO_LARGE);
  if (plum_validate_palette_indexes(image)) throw(&context, PLUM_ERR_INVALID_COLOR_INDEX);
  switch (image -> type) {
    case PLUM_IMAGE_BMP: generate_BMP_data(&context); break;
    default: throw(&context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
	size_t output_size = get_total_output_size(&context);
  if (!output_size) throw(&context, PLUM_ERR_INVALID_IMAGE_DATA);
  switch (size) {
    case PLUM_FILENAME:
      write_generated_image_data_to_file(&context, buffer);
      break;
    case PLUM_BUFFER: {
      void * out = malloc(output_size);
      if (!out) throw(&context, PLUM_ERR_OUT_OF_MEMORY);
      *(struct plum_buffer *) buffer = (struct plum_buffer) {.size = output_size, .data = out};
      write_generated_image_data(out, context.output);
    } break;
    default:
      if (output_size > size) throw(&context, PLUM_ERR_IMAGE_TOO_LARGE);
      write_generated_image_data(buffer, context.output);
  }
  context.size = output_size;
  done:
  destroy_allocator_list(context.allocator);
  if (error) *error = context.status;
  return context.size;
}

void write_generated_image_data_to_file (struct context * context, const char * filename) {
  FILE * fp = fopen(filename, "wb");
  if (!fp) throw(context, PLUM_ERR_FILE_INACCESSIBLE);
  const struct data_node * node;
  for (node = context -> output; node -> previous; node = node -> previous);
  while (node) {
    const char * data = node -> data;
    size_t size = node -> size;
    while (size) {
      unsigned count = fwrite(data, 1, (size > 0x4000) ? 0x4000 : size, fp);
      if (count) {
        data += count;
        size -= count;
      } else {
        fclose(fp);
        throw(context, PLUM_ERR_FILE_ERROR);
      }
    }
    node = node -> next;
  }
  fclose(fp);
}

void write_generated_image_data (void * restrict buffer, const struct data_node * data) {
  const struct data_node * node;
  for (node = data; node -> previous; node = node -> previous);
  char * out = buffer;
  while (node) {
    memcpy(out, node -> data, node -> size);
    out += node -> size;
    node = node -> next;
  }
}

size_t get_total_output_size (struct context * context) {
  size_t result = 0;
  const struct data_node * node;
  for (node = context -> output; node; node = node -> previous) {
    if ((result + node -> size) < result) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
    result += node -> size;
  }
  return result;
}
