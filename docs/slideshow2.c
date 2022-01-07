// This program is referenced by the tutorial at tutorial.md, chapter 10. See that page for more information.

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#define PLUM_UNPREFIXED_MACROS
#include "libplum.h"

int main (int argc, char ** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <output> <input> [<input> ...]\n", *argv);
    return 2;
  }
  unsigned error;
  // load the first image, to determine the size of the output canvas
  struct plum_image * input = plum_load_image(argv[2], PLUM_FILENAME, PLUM_COLOR_32 | PLUM_ALPHA_INVERT, &error);
  if (error) {
    fprintf(stderr, "error: loading %s: %s\n", argv[2], plum_get_error_text(error));
    return 1;
  }
  // generate the output image
  struct plum_image output = {
    .type = PLUM_IMAGE_APNG, // change to PLUM_IMAGE_GIF if desired
    .width = input -> width,
    .height = input -> height,
    .frames = argc - 2,
    .color_format = PLUM_COLOR_32 | PLUM_ALPHA_INVERT // use PLUM_ALPHA_INVERT so that zero is transparent
  };
  if (!plum_check_valid_image_size(output.width, output.height, output.frames)) {
    // plum_check_valid_image_size checks whether the chosen dimensions can cause size calculations to overflow
    fputs("error: output file too big\n", stderr);
    plum_destroy_image(input);
    return 1;
  }
  // calloc initializes all pixels (of all frames) to 0, so that borders for smaller images remain transparent black
  output.data32 = plum_calloc(&output, sizeof *output.data32 * output.width * output.height * output.frames);
  uint32_t PIXARRAY(outpixels, &output) = (void *) output.data32;
  // generate the output image's metadata: loop count, frame durations and disposals, and background color
  uint32_t frame, row, col, zero = 0; // define a zero variable so that &zero points to a uint32_t 0
  uint64_t * frame_durations = malloc(output.frames * sizeof *frame_durations);
  uint8_t * frame_disposals = malloc(output.frames * sizeof *frame_disposals);
  if (!(output.data32 && frame_durations && frame_disposals)) {
    // don't bother releasing any memory if allocations fail; just exit quickly
    fputs("error: out of memory\n", stderr);
    return 3;
  }
  for (frame = 0; frame < output.frames; frame ++) {
    frame_durations[frame] = 750000000; // 3/4 of a second per frame
    frame_disposals[frame] = PLUM_DISPOSAL_BACKGROUND; // restore the background to avoid blending frames
  }
  error = plum_append_metadata(&output, PLUM_METADATA_BACKGROUND, &zero, sizeof zero); // 0 = transparent black
  if (!error) error = plum_append_metadata(&output, PLUM_METADATA_FRAME_DURATION,
                                           frame_durations, output.frames * sizeof *frame_durations);
  free(frame_durations); // no longer needed once it has been added to the image as metadata
  if (!error) error = plum_append_metadata(&output, PLUM_METADATA_FRAME_DISPOSAL,
                                           frame_disposals, output.frames * sizeof *frame_disposals);
  free(frame_disposals); // likewise
  if (!error) error = plum_append_metadata(&output, PLUM_METADATA_LOOP_COUNT, &zero, sizeof zero); // 0 = loop forever
  if (error) {
    fprintf(stderr, "error: preparing animation: %s\n", plum_get_error_text(error));
    plum_destroy_image(input);
    plum_destroy_image(&output);
    return 1;
  }
  // copy the loaded frame and load the next frame
  for (frame = 0; frame < output.frames; frame ++) {
    uint32_t PIXARRAY(inpixels, input) = input -> data;
    uint32_t width = (input -> width < output.width) ? input -> width : output.width;
    uint32_t height = (input -> height < output.height) ? input -> height : output.height;
    for (row = 0; row < height; row ++) for (col = 0; col < width; col ++)
      outpixels[frame][row][col] = inpixels[0][row][col];
    plum_destroy_image(input);
    if (argv[frame + 3]) {
      // load the next frame, if there is one
      input = plum_load_image(argv[frame + 3], PLUM_FILENAME, PLUM_COLOR_32 | PLUM_ALPHA_INVERT, &error);
      if (error) {
        fprintf(stderr, "error: loading %s: %s\n", argv[frame + 3], plum_get_error_text(error));
        plum_destroy_image(&output);
        return 1;
      }
    }
  }
  // all frames are loaded: write out the output file
  plum_store_image(&output, argv[1], PLUM_FILENAME, &error);
  if (error) fprintf(stderr, "error: generating %s: %s\n", argv[1], plum_get_error_text(error));
  // release all allocated data linked to the image (pixel data and metadata) and exit
  plum_destroy_image(&output);
  return !!error;
}
