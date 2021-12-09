# Overview

This library handles image data in a number of different formats, allowing users to read it, manipulate it and write
it through a unified interface.
The main goal is to provide a simple interface through which the most common operations can be easily performed.
It does **not** attempt to generate or give fine control over every possible feature of every file format it supports;
in most cases, while it will read files using all sorts of optional features, it will discard most non-image data and
it will automatically choose the best possible configuration when generating an image file.
However, [indexed-color images][indexed] (i.e., images using a color palette) are fully supported and considered a
first-class use case for the library.

The API is designed to provide the minimal functionality needed to read, write and generate image data.
Some functions are provided because they match functionality that was needed internally (for instance, rotating images
was necessary to load JPEG images that contain an orientation parameter, so the [`plum_rotate_image`][rotate] function
was exposed in the API), but generic image edition capabilities (such as cropping or interpolating colors) are outside
the library's scope.

The library automatically detects image formats when loading images, and allows generating image files through the
same interface for all image formats.
For example, the following is a complete program that will convert any image file to PNG (including error checking):

``` c
#include <stdio.h>
#include "libplum.h"

int main (int argc, char ** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <input> <output.png>\n", *argv);
    return 2;
  }
  unsigned error;
  struct plum_image * image = plum_load_image(argv[1], PLUM_FILENAME, PLUM_COLOR_64 | PLUM_PALETTE_LOAD, &error);
  if (image) {
    image -> type = PLUM_IMAGE_PNG;
    plum_store_image(image, argv[2], PLUM_FILENAME, &error);
    plum_destroy_image(image);
  }
  if (error) fprintf(stderr, "error: %s\n", plum_get_error_text(error));
  return !!error;
}
```

(Note: the `PLUM_COLOR_64` constant indicates the color format that will be used for the image data while in memory;
see the [Color formats][formats] page for more details.
The `PLUM_PALETTE_LOAD` constant indicates that the library should load the image as an indexed-color image if it does
have a palette.)

As it can be seen in the example, the library doesn't allow the user to add comments to the image file, configure the
pixel format that will be output, or adjust the compression; it will add no comments and choose the pixel format and
the compression level automatically. This library is not intended for dedicated image editors, which will want fine
control over all of those features; rather, it is intended for applications that need to process images to serve some
other purpose, and that would therefore prefer a simple and unified interface to handle their image data.

The library contains no multithreading support; code always runs in the same thread that invoked it.
However, since it also contains no global mutable state, it can be used safely by multiple threads simultaneously, as
long as they aren't performing mutable operations on the same image.

* * *

Next: [Building and including the library](building.md)

Up: [README](README.md)

[formats]: colors.md
[indexed]: colors.md#indexed-color-mode
[rotate]: functions.md#plum_rotate_image
