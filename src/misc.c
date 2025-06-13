#include "proto.h"

unsigned plum_validate_image (const struct plum_image * image) {
  if (!image) return PLUM_ERR_INVALID_ARGUMENTS;
  if (!(image -> width && image -> height && image -> frames && image -> data)) return PLUM_ERR_NO_DATA;
  if (!plum_check_valid_image_size(image -> width, image -> height, image -> frames)) return PLUM_ERR_IMAGE_TOO_LARGE;
  if (image -> type >= PLUM_NUM_IMAGE_TYPES) return PLUM_ERR_INVALID_FILE_FORMAT;
  bool found[PLUM_NUM_METADATA_TYPES - 1] = {0};
  for (const struct plum_metadata * metadata = image -> metadata; metadata; metadata = metadata -> next) {
    if (metadata -> size && !metadata -> data) return PLUM_ERR_INVALID_METADATA;
    if (metadata -> type <= 0) continue;
    if (metadata -> type >= PLUM_NUM_METADATA_TYPES || found[metadata -> type - 1]) return PLUM_ERR_INVALID_METADATA;
    found[metadata -> type - 1] = true;
    switch (metadata -> type) {
      case PLUM_METADATA_COLOR_DEPTH:
        if (metadata -> size < 3 || metadata -> size > 5) return PLUM_ERR_INVALID_METADATA;
        break;
      case PLUM_METADATA_BACKGROUND:
        if (metadata -> size != plum_color_buffer_size(1, image -> color_format)) return PLUM_ERR_INVALID_METADATA;
        break;
      case PLUM_METADATA_LOOP_COUNT:
        if (metadata -> size != sizeof(uint32_t)) return PLUM_ERR_INVALID_METADATA;
        break;
      case PLUM_METADATA_FRAME_DURATION:
        if (metadata -> size % sizeof(uint64_t)) return PLUM_ERR_INVALID_METADATA;
        break;
      case PLUM_METADATA_FRAME_DISPOSAL:
        for (size_t p = 0; p < metadata -> size; p ++) if (p[(const uint8_t *) metadata -> data] >= PLUM_NUM_DISPOSAL_METHODS) return PLUM_ERR_INVALID_METADATA;
        break;
      case PLUM_METADATA_FRAME_AREA: {
        const struct plum_rectangle * rectangles = metadata -> data;
        if (metadata -> size % sizeof *rectangles) return PLUM_ERR_INVALID_METADATA;
        uint_fast32_t frames = (image -> frames > metadata -> size / sizeof *rectangles) ? metadata -> size / sizeof *rectangles : image -> frames;
        for (uint_fast32_t frame = 0; frame < frames; frame ++) {
          if (!(rectangles[frame].width && rectangles[frame].height)) return PLUM_ERR_INVALID_METADATA;
          uint32_t right = rectangles[frame].left + rectangles[frame].width, bottom = rectangles[frame].top + rectangles[frame].height;
          if (right < rectangles[frame].left || right > image -> width || bottom < rectangles[frame].top || bottom > image -> height)
            return PLUM_ERR_INVALID_METADATA;
        }
      }
    }
  }
  return 0;
}

const char * plum_get_error_text (unsigned error) {
  static const char * const messages[PLUM_NUM_ERRORS] = {
    [PLUM_OK]                      = "success",
    [PLUM_ERR_INVALID_ARGUMENTS]   = "invalid argument for function",
    [PLUM_ERR_INVALID_FILE_FORMAT] = "invalid image data or unknown format",
    [PLUM_ERR_INVALID_METADATA]    = "invalid image metadata",
    [PLUM_ERR_INVALID_COLOR_INDEX] = "invalid palette index",
    [PLUM_ERR_TOO_MANY_COLORS]     = "too many colors in image",
    [PLUM_ERR_UNDEFINED_PALETTE]   = "image palette not defined",
    [PLUM_ERR_IMAGE_TOO_LARGE]     = "image dimensions too large",
    [PLUM_ERR_NO_DATA]             = "image contains no image data",
    [PLUM_ERR_NO_MULTI_FRAME]      = "multiple frames not supported",
    [PLUM_ERR_FILE_INACCESSIBLE]   = "could not access file",
    [PLUM_ERR_FILE_ERROR]          = "file input/output error",
    [PLUM_ERR_OUT_OF_MEMORY]       = "out of memory"
  };
  if (error >= PLUM_NUM_ERRORS) return NULL;
  return messages[error];
}

const char * plum_get_file_format_name (unsigned format) {
  static const char * const formats[PLUM_NUM_IMAGE_TYPES] = {
    [PLUM_IMAGE_NONE] = NULL, // default for invalid formats
    [PLUM_IMAGE_BMP]  = "BMP",
    [PLUM_IMAGE_GIF]  = "GIF",
    [PLUM_IMAGE_PNG]  = "PNG",
    [PLUM_IMAGE_APNG] = "APNG",
    [PLUM_IMAGE_JPEG] = "JPEG",
    [PLUM_IMAGE_PNM]  = "PNM",
    [PLUM_IMAGE_QOI]  = "QOI"
  };
  if (format >= PLUM_NUM_IMAGE_TYPES) format = PLUM_IMAGE_NONE;
  return formats[format];
}

uint32_t plum_get_version_number (void) {
  return PLUM_VERSION;
}
