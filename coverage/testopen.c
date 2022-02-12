#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libplum.h"

int readcb (void * fp, void * buffer, int size) {
  int rv = fread(buffer, 1, size, fp);
  if (ferror(fp)) return -1;
  return rv;
}

int writecb (void * fp, void * buffer, int size) {
  if (feof(fp)) return -1;
  int rv = fwrite(buffer, 1, size, fp);
  if (ferror(fp)) return -1;
  return rv;
}

int main (int argc, char ** argv) {
  unsigned error;
  if (argc > 4) {
    fprintf(stderr, "usage: %s [input] [output] [format]\n", *argv);
    return 2;
  }
  unsigned long format = 0;
  unsigned palette = PLUM_PALETTE_LOAD, rotation = 0, flip = 0, flags = 0;
  if (argc >= 4) {
    const char * formatcode = argv[3];
    for (; *formatcode && !strchr("0123456789", *formatcode); formatcode ++) switch (*formatcode) {
      case 'd': flags |= PLUM_SORT_DARK_FIRST; break;
      case 'f': flip = 1; break;
      case 'g': palette = PLUM_PALETTE_GENERATE; break;
      case 'k': palette = PLUM_PALETTE_NONE; break;
      case 'l': rotation = -1; break;
      case 'p': flags |= PLUM_PALETTE_REDUCE; break;
      case 'r': rotation = 1; break;
      case 's': flags |= PLUM_SORT_EXISTING; break;
      case 't': flags |= PLUM_ALPHA_REMOVE; break;
      case 'u': rotation = 2; break;
      default:
        fprintf(stderr, "error: unknown format character: %c\n", *formatcode);
        return 2;
    }
    if (*formatcode) {
      if (strspn(formatcode, "0123456789") != strlen(formatcode)) {
        fprintf(stderr, "error: invalid format number: %s\n", formatcode);
        return 2;
      }
      format = strtoul(formatcode, NULL, 10);
    }
  }
  struct plum_image * image;
  if ((argc >= 2) && strcmp(argv[1], "-"))
    image = plum_load_image(argv[1], PLUM_MODE_FILENAME, PLUM_COLOR_64 | palette | flags, &error);
  else {
    clearerr(stdin);
    image = plum_load_image((const struct plum_callback []) {{.callback = &readcb, .userdata = stdin}}, PLUM_MODE_CALLBACK, PLUM_COLOR_64 | palette, &error);
  }
  if (!image) {
    fprintf(stderr, "load error: %s\n", plum_get_error_text(error));
    return 1;
  }
  if (rotation || flip) {
    error = plum_rotate_image(image, rotation, flip);
    if (error) {
      fprintf(stderr, "rotate error: %s\n", plum_get_error_text(error));
      plum_destroy_image(image);
      return 1;
    }
  }
  size_t result = 0;
  if (argc > 2) {
    if (format) image -> type = format;
    if (strcmp(argv[2], "-"))
      result = plum_store_image(image, argv[2], PLUM_MODE_FILENAME, &error);
    else {
      clearerr(stdout);
      result = plum_store_image(image, (struct plum_callback []) {{.callback = &writecb, .userdata = stdout}}, PLUM_MODE_CALLBACK, &error);
    }
    if (!result) fprintf(stderr, "store error: %s\n", plum_get_error_text(error));
  }
  plum_destroy_image(image);
  if (result) fprintf(stderr, "output size: %zu\n", result);
  return !!error;
}
