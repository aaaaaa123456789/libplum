# Introduction and tutorial

This page aims to give an introduction to how the library works and how to use it to perform some common tasks,
including working code samples.
Detailed information about the library can be found in the rest of the documentation.

All code samples are released to the public domain, just like the rest of the repository.

Note: all code samples use C, not C++, unless otherwise noted.

1. [Loading an image](#1-loading-an-image)
2. [Storing an image](#2-storing-an-image)
3. [Accessing pixel data](#3-accessing-pixel-data)
4. [Using pixel coordinates](#4-using-pixel-coordinates)
5. [Palettes and indexed-color mode](#5-palettes-and-indexed-color-mode)
6. [Metadata](#6-metadata)
7. [Animations](#7-animations)
8. [Color and palette conversions](#8-color-and-palette-conversions)
9. [Memory management](#9-memory-management)
10. [Accessing images not in files](#10-accessing-images-not-in-files)
11. [Further resources](#11-further-resources)

## 1. Loading an image

Most of the functionality of the library is provided by two functions, [`plum_load_image`][load] and
[`plum_store_image`][store].
These two functions allow, respectively, loading image file data and writing it out.

In the program, image data is represented by a [`plum_image`][image] struct.
This struct contains the image's pixel data, as well as some ancillary information like its dimensions; its full
contents can be found in the corresponding reference page.
(Note: all function, type, constant and macro names link to their corresponding reference pages; click on them for
a full description of them.)

The library's main design concept is to provide a unified interface for all image data.
Therefore, a [`plum_image`][image] struct will contain image data for any format; when an image is loaded, the format
it used will be written to its `type` member, and that same member will determine which format will be generated when
the image data is written out.

The first part of this tutorial will focus on loading and storing images to files.
Loading an image from a file and checking some of its attributes is very simple:

``` c
#include <stdio.h>
#include "libplum.h"

int main (int argc, char ** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <image>\n", *argv);
    return 2;
  }
  unsigned error;
  struct plum_image * image = plum_load_image(argv[1], PLUM_FILENAME,
                                              PLUM_COLOR_32, &error);
  if (image) {
    const char * format = plum_get_file_format_name(image -> type);
    printf("%s: %s image, %lu x %lu px", argv[1], format,
                                         (unsigned long) image -> width,
                                         (unsigned long) image -> height);
    if (image -> frames > 1)
      printf(", %lu frames", (unsigned long) image -> frames);
    putchar('\n');
    plum_destroy_image(image);
  } else
    printf("%s: load error (%s)\n", argv[1], plum_get_error_text(error));
  return !image; // 0 if it was valid, 1 if not
}
```

A few things can be noted in this example:

- When the program is done using an image, it can release all its resources with [`plum_destroy_image`][destroy].
  This will free everything allocated by the library for that image — including, in this case, the image struct
  itself.
- When loading an image, the program must specify the color format it will use.
  The library supports four different [color formats][color-formats], as well as a fifth pixel format for images using
  palettes (referred to as [indexed-color mode][indexed] in this documentation).
  (For images using palettes, the color format indicates the format of the palette data, not the pixel data.)
  The user must request which color format they want to operate with; the library will load the image and convert to
  the chosen color format, regardless of the actual representation used internally by the image file.
  The format chosen in this example, [`PLUM_COLOR_32`][loading-flags], uses 32-bit (`uint32_t`) color values, encoded
  as RGBA 8.8.8.8 (with the red channel in the least significant bits), and with the alpha channel inverted (i.e., 0
  means opaque) for convenience when dealing with images without transparency.
- Some functions can return an error constant describing why they failed.
  The [`plum_load_image`][load] function does so via a return argument (which may be `NULL` if the caller doesn't care
  about the reason for failure).
  Error constants are always positive, and no error (i.e., [`PLUM_OK`][errors]) is zero, so it is possible to test
  whether an error occurred by testing this value.
- At no point does the user tell the library the type of file being loaded.
  Image formats are autodetected: all supported types have distinguishable headers, and thus the library can
  automatically determine which format is being loaded.
  Therefore, any user of the library will automatically be able to handle all supported image file formats out of the
  box.
- Images can have more than one frame.
  This is necessary for formats that contain animations (such as GIF), but also valid in some non-animated formats
  (such as the PNM family of formats).
  If an image contains more than one frame, all frames will have the same dimensions.

## 2. Storing an image

The counterpart to [`plum_load_image`][load] is [`plum_store_image`][store], a function that converts the image data
from a [`plum_image`][image] struct into actual image data in some file format and stores that data.
(Much like [`plum_load_image`][load], [`plum_store_image`][store] can store image data in many different locations,
but the first part of this tutorial will focus on files.)
The file format will be determined by the image's `type` member; the remaining members will determine the
characteristics of the image file that will be generated.

Not all file formats can contain all of the information that a [`struct plum_image`][image] can hold.
The library will store as much as it can possibly store in that format.
For example:

- BMP files are limited to 32 bits per pixel; if an image's color depth is higher, it will be reduced.
- JPEG compression is lossy, and therefore, the generated image won't be an exact representation of the original.
- Images can contain a certain amount of [metadata][metadata].
  However, only the metadata supported by the chosen format will be stored.

However, limitations that would completely prevent parts of the image from being stored properly will cause the
conversion to fail.
For example:

- GIF files may only contain up to 256 colors per frame.
  An image with more will fail to convert with a [`PLUM_ERR_TOO_MANY_COLORS`][errors] error.
- JPEG files are limited to 65,535 pixels in each dimension.
  Larger images will fail to convert with a [`PLUM_ERR_IMAGE_TOO_LARGE`][errors] error.
- Many file formats only support a single image (i.e., a single frame).
  Images with two or more frames will fail to convert to those formats with a [`PLUM_ERR_NO_MULTI_FRAME`][errors]
  error.

This sample program converts an image to PNG:

``` c
#include <stdio.h>
#include "libplum.h"

int main (int argc, char ** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <input> <output.png>\n", *argv);
    return 2;
  }
  unsigned error;
  struct plum_image * image = plum_load_image(argv[1], PLUM_FILENAME,
                                              PLUM_COLOR_32, &error);
  if (!image) {
    fprintf(stderr, "load error: %s\n", plum_get_error_text(error));
    return 1;
  }
  image -> type = PLUM_IMAGE_PNG;
  plum_store_image(image, argv[2], PLUM_FILENAME, &error);
  plum_destroy_image(image);
  if (error) {
    fprintf(stderr, "store error: %s\n", plum_get_error_text(error));
    return 1;
  }
  return 0;
}
```

As the example shows, no configuration is needed to generate an image file.
The image data is already contained in the [`plum_image`][image] struct, and the library will automatically choose any
parameters that the chosen file format requires when generating the image file.

The image conversion may fail: for example, if the loaded image file is a GIF animation with multiple frames, it
cannot be encoded as a PNG.
(The library does support APNG, but it is treated as a separate format; the `type` member would have to be set to
[`PLUM_IMAGE_APNG`][format-constants] to emit an APNG file.)
In that case, [`plum_store_image`][store] will fail with an error.
This doesn't necessarily indicate that the image is invalid: the image data can be validated with the
[`plum_validate_image`][validate] function, and all images loaded by [`plum_load_image`][load] will be reported as
valid by that function.
Instead, it indicates that the conversion to the chosen file format (in this case, PNG) failed for some reason.

## 3. Accessing pixel data

Images' pixel data is stored in the `data` member of the [`plum_image`][image] struct.
This member is a three-dimensional array of color values, where the dimensions are the number of frames, the image's
height, and the image's width.

Pixel data is just a series of color values (or palette indexes for images using [indexed-color mode][indexed]; that
mode will be explained [in a later section](#5-palettes-and-indexed-color-mode)); color values can take one of four
[different formats][color-formats], chosen when the image is created/loaded.
(This is the meaning of the [`PLUM_COLOR_32`][loading-flags] constant used previously as an argument to the
[`plum_load_image`][load] function.)

The four available [color formats][color-formats] are:

- [`PLUM_COLOR_32`][loading-flags]: RGBA 8.8.8.8, `uint32_t` color values
- [`PLUM_COLOR_64`][loading-flags]: RGBA 16.16.16.16, `uint64_t` color values
- [`PLUM_COLOR_16`][loading-flags]: RGBA 5.5.5.1, `uint16_t` color values
- [`PLUM_COLOR_32X`][loading-flags]: RGBA 10.10.10.2, `uint32_t` color values

In all cases, the components are listed LSB first (i.e., the least significant bits always correspond to the red
component).
The suffixes `32`, `64`, `16` and `32X` are used consistently to represent these four formats throughout the library.
(Where only data width matters, not the actual bit layout, `32` is also used for the `32X` format.)

The alpha channel is inverted by default: 0 means opaque, and a maximum value means fully transparent.
This allows programs that don't use transparency to leave it at zero without accidentally creating a fully-transparent
image.
However, since programs that do use transparency might find this convention inconvenient, it is possible to combine
these color formats with the [`PLUM_ALPHA_INVERT`][loading-flags] flag through a bitwise OR with the color format
constant.
(For example, the `PLUM_COLOR_32 | PLUM_ALPHA_INVERT` color format uses 0 as fully transparent and 255 (highest 8-bit
value) as fully opaque.)
Everything explained for these color formats also works with their alpha-inverted variants; they merely use opposite
alpha values.

Given the descriptions above, it is perfectly possible to construct color values by hand.
For example, in the [`PLUM_COLOR_32`][loading-flags] color format, a color value of `0x0000ffff` (maximum red and
green, zero blue and alpha) would represent solid yellow.
However, to make color calculations simpler, the library offers a number of [macros][color-macros].
These macros are also available as shorter, [unprefixed macros][unprefixed], intended to make user code more readable;
in order to make unprefixed macros available, `#define` the [`PLUM_UNPREFIXED_MACROS`][feature-macros] constant before
including the library header.

Macros are available for each color format; they differ only in their suffix.
The macros for the [`PLUM_COLOR_32`][loading-flags] color format are:

- [`PLUM_COLOR_VALUE_32`][color-macros]: takes four components (red, green, blue, alpha) and generates a single color
  value from them; the resulting value is of the right type (`uint32_t` in this case).
  Short form: [`COLOR32`][unprefixed].
- [`PLUM_RED_32`][color-macros]: takes a color value and extracts its red component; the resulting value will still be
  of the type expected by the color format (`uint32_t` in this case).
  Similar macros exist for the other three components; replace `RED` with `GREEN`, `BLUE` or `ALPHA`.
  Short form: [`RED32`][unprefixed].

While the image's pixel data is accessible through its `data` member, that member is of `void *` type, making it
inconvenient to access the pixels directly.
Therefore, in sufficiently recent versions of C and C++ (C99 onwards and all standard versions of C++), the library
header defines aliases for this member, `data16`, `data32` and `data64`, which are pointers to the correct integer
type.
(There is also a `data8` alias for [indexed-color mode][indexed], but that will be explained in a later section.)

Using all of these elements, the following program will darken an image by reducing its color components by 10%:

``` c
#include <stdio.h>
#include <stddef.h>
#define PLUM_UNPREFIXED_MACROS
#include "libplum.h"

#define COEF 90 /* multiply all color values by 90% */

int main (int argc, char ** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename>\n", *argv);
    return 2;
  }
  unsigned error;
  struct plum_image * image = plum_load_image(argv[1], PLUM_FILENAME,
                                              PLUM_COLOR_64, &error);
  if (!image) {
    fprintf(stderr, "load error: %s\n", plum_get_error_text(error));
    return 1;
  }
  size_t index;
  size_t size = (size_t) image -> width * image -> height * image -> frames;
  for (index = 0; index < size; index ++)
    image -> data64[index] = COLOR64(
      (RED64(image -> data64[index]) * COEF + 50) / 100,
      (GREEN64(image -> data64[index]) * COEF + 50) / 100,
      (BLUE64(image -> data64[index]) * COEF + 50) / 100,
      ALPHA64(image -> data64[index])
    );
  plum_store_image(image, argv[1], PLUM_FILENAME, &error);
  plum_destroy_image(image);
  if (!error) return 0;
  fprintf(stderr, "store error: %s\n", plum_get_error_text(error));
  return 1;
}
```

It should be noted that the program above accesses the pixel data through `image -> data64` as a linear array;
`data64` is just a `uint64_t *` member.
This is simple when a program merely needs to iterate through all pixels in an image.
However, it can be inconvenient when a program needs to know the location of the pixels it is accessing.

While pixel data is laid out in the obvious order (frame by frame, row by row, top to bottom, left to right, and
without gaps), and the way to access it [is well defined][accessing], accessing it through computed array indexes
isn't always practical, which leads to the next section of this tutorial.

## 4. Using pixel coordinates

TBD

## 5. Palettes and indexed-color mode

TBD

## 6. Metadata

TBD

## 7. Animations

TBD

## 8. Color and palette conversions

TBD

## 9. Memory management

TBD

## 10. Accessing images not in files

TBD

## 11. Further resources

TBD

* * *

Prev: [Overview](overview.md)

Next: [Building and including the library](building.md)

Up: [README](README.md)

[accessing]: colors.md#accessing-pixel-and-color-data
[color-formats]: colors.md#formats
[color-macros]: macros.md#color-macros
[color-masks]: constants.md#color-mask-constants
[destroy]: functions.md#plum_destroy_image
[errors]: constants.md#errors
[feature-macros]: macros.md#feature-test-macros
[format-constants]: constants.md#image-types
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[load]: functions.md#plum_load_image
[loading-flags]: constants.md#loading-flags
[metadata]: metadata.md
[store]: functions.md#plum_store_image
[unprefixed]: macros.md#unprefixed-macros
[validate]: functions.md#plum_validate_image
