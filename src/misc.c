#include "proto.h"

const char * plum_get_error_text (unsigned error) {
  static const char * const messages[] = {
    [PLUM_OK]                      = "success",
    [PLUM_ERR_INVALID_ARGUMENTS]   = "invalid argument for function",
    [PLUM_ERR_INVALID_FILE_FORMAT] = "invalid image data or unknown format",
    [PLUM_ERR_INVALID_IMAGE_DATA]  = "invalid image data",
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
  static const char * const formats[] = {
    [PLUM_IMAGE_NONE] = NULL, // default for invalid formats
    [PLUM_IMAGE_BMP]  = "BMP",
    [PLUM_IMAGE_GIF]  = "GIF",
    [PLUM_IMAGE_PNG]  = "PNG",
    [PLUM_IMAGE_APNG] = "APNG"
  };
  if (format >= PLUM_NUM_IMAGE_TYPES) format = PLUM_IMAGE_NONE;
  return formats[format];
}

int compare64 (const void * first, const void * second) {
  const uint64_t * p1 = first;
  const uint64_t * p2 = second;
  return (*p1 > *p2) - (*p1 < *p2);
}
