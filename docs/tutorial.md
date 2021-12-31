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
9. [Generating images from scratch](#9-generating-images-from-scratch)
10. [Memory management](#10-memory-management)
11. [Accessing images not in files](#11-accessing-images-not-in-files)
12. [Further resources](#12-further-resources)

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

When working with image data, it is almost always desirable to be able to address that data using its coordinates.
Since this library allows handling multi-frame images, a pixel is always identified by three coordinates: the column
(i.e., X coordinate), the row (i.e., Y coordinate) and the frame number.
All coordinates start counting from 0 at the top-left corner of the first frame of the image.

For all examples in this section, an image using the [`PLUM_COLOR_32`][loading-flags] color format will be used.
To access data of a different bit width, replace all instances of `32` with `64` or `16` (or `8` for images using
[indexed-color mode][indexed], as it will be shown in a later section).
Note that images using the [`PLUM_COLOR_32X`][loading-flags] color format will use the same mechanisms described here,
since the underlying data type (`uint32_t`) is the same.

It is entirely possible to access a pixel directly using its coordinates, since the pixels are laid out sequentially,
one frame after the other, one row after the other.
For example:

``` c
uint32_t get_color_at_coordinates (const struct plum_image * image,
                                   uint32_t col, uint32_t row, uint32_t frame) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  return image -> data32[((size_t) frame * image -> height + row)
                         * image -> width + col];
}
```

(Note that the cast from `uint32_t` to `size_t` may be needed to prevent overflow on particularly large images.)

However, having to do this every time is inconvenient.
The library therefore offers several convenience mechanisms that address a single pixel from its coordinates.
All of them result in an lvalue, i.e., a value that can also be used to assign a new color, not just read it.

The simplest method is to just use the [`PLUM_PIXEL_INDEX`][pixel-index] macro, which is also available as `PIXEL`
if [unprefixed macros][unprefixed] are enabled.
This macro just takes the image and the three coordinates as arguments, and returns a `size_t` value that can be used
as an array index, like so:

``` c
uint32_t get_color_at_coordinates (const struct plum_image * image,
                                   uint32_t col, uint32_t row, uint32_t frame) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  return image -> data32[PIXEL(image, col, row, frame)];
}
```

It is also possible to use a macro that expands to the pixel value directly; there is a different macro for each
possible bit width.
For the 32-bit data example, the corresponding macro is [`PLUM_PIXEL_32`][pixel-index], or `PIXEL32` if
[unprefixed macros][unprefixed] are enabled.
This macro is good for both reading from a pixel and assigning a new value to it.
For example, the following snippet will invert all the colors along an image's main diagonal:

``` c
void invert_diagonal (struct plum_image * image) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  uint32_t p, frame, limit = (image -> width < image -> height) ?
                               image -> width : image -> height;
  uint32_t mask = PLUM_RED_MASK_32 | PLUM_GREEN_MASK_32 | PLUM_BLUE_MASK_32;
  for (frame = 0; frame < image -> frames; frame ++)
    for (p = 0; p < limit; p ++)
      PIXEL32(image, p, p, frame) ^= mask;
}
```

If the compiler supports VLAs (which requires using C99 or later, and isn't available in C++), it is possible to
declare a suitable array pointer to which the `data` member of the image may be converted; this array can be used to
access the pixels directly.
The [`PLUM_PIXEL_ARRAY`][pixel-array] macro (or `PIXARRAY` if [unprefixed macros][unprefixed] are enabled) will expand
to a declarator for such an array; the first argument is the name of the variable being declared, and the second
argument is the image that the declarator is based on.

(Note: C declarations are comprised of three components: _declaration specifiers_ (including the type names),
_declarators_ (including the variable names), and optional _initializers_ (self-explanatory).
This macro only expands to the declarator; it doesn't include the type name, because that depends on the color format
in use.
Therefore, the type name must be added manually, as in `uint32_t PIXARRAY(array, image);` to declare `array`.)

Since the above mechanism requires declaring a new variable, the library also provides macros to cast the image's
`data` member to the correct array type directly.
For example, for the [`PLUM_COLOR_32`][loading-flags] color format, the [`PLUM_PIXELS_32`][pixel-casts] macro (or
`PIXELS32` if [unprefixed macros][unprefixed] are enabled; note the difference with `PIXEL32`) will expand to the
image's `data` member cast to the corresponding pointer to array of `uint32_t` values.

**Note:** since arrays are indexed by their major dimension first, array indexes are backwards with respect to all
other access methods: first the frame, then the row (Y coordinate), then the column (X coordinate).

The following example rewrites the functions above using both VLA-based array macros (one for each):

``` c
uint32_t get_color_at_coordinates (const struct plum_image * image,
                                   uint32_t col, uint32_t row, uint32_t frame) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  return PIXELS32(image)[frame][row][col];
}

void invert_diagonal (struct plum_image * image) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  uint32_t p, frame, limit = (image -> width < image -> height) ?
                               image -> width : image -> height;
  uint32_t mask = PLUM_RED_MASK_32 | PLUM_GREEN_MASK_32 | PLUM_BLUE_MASK_32;
  uint32_t PIXARRAY(data, image) = image -> data;
  for (frame = 0; frame < image -> frames; frame ++)
    for (p = 0; p < limit; p ++) data[frame][p][p] ^= mask;
}
```

Finally, in C++ mode, while the macros above will not be available, the [`plum_image`][image] struct has instead some
[helper methods][methods] that can be used to access the pixel data directly.
These methods all return lvalue references (e.g., `uint32_t &` for 32-bit color formats) and exist in both `const` and
non-`const` variants.
There are separate methods for each bit width, since they return different data types; for example, for the
[`PLUM_COLOR_32`][loading-flags] color format, the method is called `pixel32`.

The following example rewrites the functions above using that method:

``` cpp
uint32_t get_color_at_coordinates (const struct plum_image * image,
                                   uint32_t col, uint32_t row, uint32_t frame) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  return image -> pixel32(col, row, frame);
}

void invert_diagonal (struct plum_image * image) {
  assert((image -> color_format & PLUM_COLOR_MASK) == PLUM_COLOR_32);
  uint32_t p, frame, limit = (image -> width < image -> height) ?
                               image -> width : image -> height;
  uint32_t mask = PLUM_RED_MASK_32 | PLUM_GREEN_MASK_32 | PLUM_BLUE_MASK_32;
  for (frame = 0; frame < image -> frames; frame ++)
    for (p = 0; p < limit; p ++) image -> pixel32(p, p, frame) ^= mask;
}
```

## 5. Palettes and indexed-color mode

Some images may use a fixed palette of colors: instead of assigning each pixel a color, they assign them an index into
a palette of colors used by the whole image.
This is called [indexed-color mode][indexed] (because pixel values are indexes), and it may be used for many reasons:
it ensures that images use a known number of colors, it makes them easier to replace globally, and it results in
smaller image files as long as they restrict themselves to a known palette, amongst others.

The library natively supports [indexed-color mode][indexed] with palettes of up to 256 colors.
Images using this mode use 8-bit (`uint8_t`) pixel data; each pixel contains an index into the image's palette.
The palette is defined by the `palette` member of the [`plum_image`][image] struct, and it contains an array of
colors, in the format defined by the image's [color format][color-formats].
(Like with the `data` member, the struct has typed aliases (in C99 and C++ mode) for this member: `palette16`,
`palette32` and `palette64`.)
The struct's `max_palette_index` member indicates the maximum valid palette index, and thus implicitly the size of the
palette (which is one greater than this value); pixel values (i.e., indexes) **must not** be larger than this value in
a valid image.

(The choice of representing the maximum valid index instead of the palette size might seem strange at first, but it
ensures that all possible values for this member are valid: the `max_palette_index` member is declared as a `uint8_t`,
and all values from 0 through 255 are valid, representing a palette with 1 to 256 colors.
The library imposes no limitation on the number of colors a palette may have other than this range: for example, it is
not required to be a power of two.)

Whether an image uses [indexed-color mode][indexed] is determined entirely by its `palette` member: if this member is
not `NULL`, then the image has a palette and uses [indexed-color mode][indexed]; if it is `NULL`, the image has no
palette and it uses direct-color mode (i.e., pixel values are colors, as shown in the examples in previous sections).
Note that the image's `max_palette_index` member is meaningless if the image doesn't use a palette.

When loading an image, whether that image will use [indexed-color mode][indexed] or not is determined by a number of
flags that can be passed to [`plum_load_image`][load].
All of these flags are ORed into the third argument (the one that contains the [color format flags][loading-flags]);
omitting them (as it has been done so far) will use the defaults for each category.

The most important flags are the ones that control whether palettes will be loaded at all:

- [`PLUM_PALETTE_NONE`][loading-flags] (default): never load a palette.
  This is the default because most programs don't expect to deal with palettes.
  If the image has a palette, it will be removed on load.
- [`PLUM_PALETTE_LOAD`][loading-flags]: load a palette if the image has one.
  This mode will faithfully reproduce the image's contents.
- [`PLUM_PALETTE_GENERATE`][loading-flags]: load a palette if the image has one, or try to generate one otherwise.
  Generating a palette will succeed if the image has 256 or fewer colors.
  This mode is intended for applications that prefer to deal with images with palettes.
- [`PLUM_PALETTE_FORCE`][loading-flags]: always load a palette; this mode is the opposite to
  [`PLUM_PALETTE_NONE`][loading-flags].
  It works like [`PLUM_PALETTE_GENERATE`][loading-flags], but fails with [`PLUM_ERR_TOO_MANY_COLORS`][errors] if it
  cannot generate a palette.
  This mode is intended for applications that only intend to deal with [indexed-color mode][indexed] images.

When generating a palette, the colors in the palette will always be sorted from brightest to darkest, with ties being
broken by transparency.
Note that the library will never dither or approximate colors in any way when generating a palette: if it attempts to
generate a palette, but the image has too many colors, it will simply fail.
When loading an image that already has a palette using the [`PLUM_PALETTE_GENERATE`][loading-flags] flag (or the 
[`PLUM_PALETTE_FORCE`][loading-flags] flag), if the image's palette has some unused colors at the end, the resulting
image's `max_palette_index` member will be adjusted to reflect the highest color index that is actually in use.
(Those modes may already generate palette data that isn't present in the image, after all, so this small change is
made to account for image file formats that only support fixed palette sizes and will pad palettes with unused colors
up to those sizes.)

Some additional flags may be ORed into that argument to specify some additional behaviors:

- [`PLUM_SORT_EXISTING`][loading-flags]: indicates that, if the image already has a palette, that palette must also be
  sorted when loaded.
  (By default, only generated palettes are sorted.)
  Note that the [`plum_sort_palette`][sort] function can also do this for already-loaded images.
- [`PLUM_SORT_DARK_FIRST`][loading-flags]: when sorting a palette (generated or existing), it indicates that it should
  sort it from darkest to brightest instead of the other way around.
- [`PLUM_PALETTE_REDUCE`][loading-flags]: when loading an existing palette, it indicates that the palette should be
  reduced by removing all duplicate and unused colors.
  Note that the [`plum_reduce_palette`][reduce] function can also do this for already-loaded images.

Putting all of this together, the following program will load an image with a palette and find its most common color:

``` c
#include <stdio.h>
#include <stddef.h>
#include "libplum.h"

#define TIE (-(size_t) 1)

int main (int argc, char ** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename>\n", *argv);
    return 2;
  }
  unsigned error;
  /* use PLUM_PALETTE_FORCE to ensure that the image will always have a    *
   * palette, and PLUM_PALETTE_REDUCE to remove duplicates from it so that *
   * they won't mess up the counts                                         */
  struct plum_image * image = plum_load_image(argv[1], PLUM_FILENAME,
    PLUM_COLOR_64 | PLUM_PALETTE_FORCE | PLUM_PALETTE_REDUCE, &error);
  if (!image) {
    fprintf(stderr, "error: %s\n", plum_get_error_text(error));
    return 1;
  }
  size_t pixels = (size_t) image -> width * image -> height * image -> frames;
  size_t counts[256] = {0};
  size_t current, max, maxcount;
  for (current = 0; current < pixels; current ++)
    counts[image -> data8[current]] ++;
  max = 0;
  maxcount = *counts; // assume the maximum is the first
  for (current = 1; current <= image -> max_palette_index; current ++)
    if (counts[current] >= maxcount) {
      if (counts[current] == maxcount)
        max = TIE;
      else
        max = current;
      maxcount = counts[current];
    }
  if (max == TIE)
    printf("Most common color: tied (%zu pixels)\n", maxcount);
  else
    printf("Most common color: 0x%016llx (%zu pixels)\n",
           (unsigned long long) image -> palette64[max], maxcount);
  plum_destroy_image(image);
  return 0;
}
```

## 6. Metadata

This library's goal is to provide a unified interface for all image file formats, and therefore, metadata is not the
primary focus.
However, a small amount of metadata is supported, since it represents data that is commonly available across several
file formats.

Metadata is accessed through the `metadata` member of the [`plum_image`][image] struct.
This member points to a linked list of [`plum_metadata`][metadata-struct] nodes; the nodes are unordered, and new
metadata may be inserted anywhere in the list.
(The library uses a linked list for this data because the number of nodes is small (typically single-digit) and
because it facilitates insertion and removal.)

Each metadata node has a `type`, which describes what kind of metadata that node represents, and a `data` and a `size`
containing the data; the layout for each metadata node is described in [the corresponding page][metadata].
(Since metadata is represented by a linked list, the `next` member points to the following node or to `NULL` for the
last node in the list, as usual.)

It is possible for users to create their own metadata types, too.
Nodes with a negative value of `type` are reserved for the user.
However, this tutorial will focus on metadata nodes with a positive `type`, since those are the ones with semantics
defined by the library.
(A `type` value of zero, represented by the [`PLUM_METADATA_NONE`][metadata-constants] constant, indicates an unused
node and may be used instead of removing the node entirely.)

An image may only contain one of each (positive) type of metadata node.
([`PLUM_METADATA_NONE`][metadata-constants] nodes and user-defined nodes may be repeated.)
The library will load and store metadata nodes whenever possible, i.e., whenever they are meaningful for the
underlying file format (and present in the image file data, when loading).

Metadata nodes defined by the library are:

- [`PLUM_METADATA_COLOR_DEPTH`][metadata-constants]: describes the actual bit depth for each color channel.
  The library will always load this node; when storing an image, this node (if present) will be used to determine the
  true color depth that should be stored.
  (If this node isn't present, the library will use the color depth determined by the [color format][color-formats]
  instead.)
- [`PLUM_METADATA_BACKGROUND`][metadata-constants]: contains a single color value indicating the image's background
  color, i.e., the color against which it should be presented.
  (Note that this node will always contain a color, even for [indexed-color mode][indexed] images.)
- [`PLUM_METADATA_LOOP_COUNT`][metadata-constants]: indicates how many times an animation will loop; defaults to 1 if
  not present.
- [`PLUM_METADATA_FRAME_DURATION`][metadata-constants]: indicates the duration of each frame in an animation, in
  nanoseconds; defaults to 0 for all frames if not present.
- [`PLUM_METADATA_FRAME_DISPOSAL`][metadata-constants]: indicates how a frame will be removed from the canvas once its
  duration expires; defaults to [`PLUM_DISPOSAL_NONE`][disposals] for all frames if not present.

The exact layout of the data for each node is described in [the Metadata page][metadata].
The last three types in that list are related to animation and will be analyzed in [a later section](#7-animations).

The [`plum_find_metadata`][find-metadata] function can be used to find a metadata node in an image; this is a simple
search through the linked list, but it is available as a convenience function so that users aren't required to
reimplement this functionality.

This function will return an image's background color, or a default value if none is defined:

``` c
uint32_t background_color (const struct plum_image * image, uint32_t dflt) {
  assert(image -> color_format == PLUM_COLOR_32);
  const struct plum_metadata * metadata =
    plum_find_metadata(image, PLUM_METADATA_BACKGROUND);
  if (!metadata) return dflt;
  return *(const uint32_t *) (metadata -> data);
}
```

And this function will replace an image's background color with yellow before storing it:

``` c
size_t store_yellow_image (struct plum_image * image, void * buffer,
                           size_t size, unsigned * restrict error) {
  // note: the arguments replicate plum_store_image's arguments
  if (!image) {
    if (error) *error = PLUM_ERR_INVALID_ARGUMENTS;
    return 0;
  }
  assert(image -> color_format == PLUM_COLOR_32);
  // create a new metadata node, to be inserted at the head of the list
  uint32_t yellow = 0xffff; // color value for yellow
  struct plum_metadata new_metadata = {
    .next = image -> metadata,
    .type = PLUM_METADATA_BACKGROUND,
    .size = sizeof yellow,
    .data = &yellow
  };
  // if there was a background, suppress it temporarily
  struct plum_metadata * old_metadata =
    plum_find_metadata(image, PLUM_METADATA_BACKGROUND);
  if (old_metadata) old_metadata -> type = PLUM_METADATA_NONE;
  image -> metadata = &new_metadata;
  size_t result = plum_store_image(image, buffer, size, error);
  // restore the old metadata, and restore the old background if needed
  image -> metadata = new_metadata.next;
  if (old_metadata) old_metadata -> type = PLUM_METADATA_BACKGROUND;
  return result;
}
```

## 7. Animations

Since the library supports multi-frame images, it follows that it is capable of handling animated images.
Image file formats such as GIF and APNG can specify an animation as a sequence of image frames, and the library can
load and generate such files.

Animations are represented as multi-frame images, using [metadata nodes][metadata] to contain the animation
parameters.
If a file format supports animations, whenever an image contains animation metadata, it will be used to generate the
corresponding animation when generating the image file; otherwise, defaults will be used.

The [metadata nodes][metadata] containing animation parameters are:

- [`PLUM_METADATA_LOOP_COUNT`][metadata-constants]: contains a single `uint32_t` value indicating how many times the
  animation will loop; if this value is 0, the animation loops forever.
  If this value is missing, the animation doesn't loop at all; this is equivalent to a loop count of 1.
- [`PLUM_METADATA_FRAME_DURATION`][metadata-constants]: array of `uint64_t` values containing the durations for each
  frame, in nanoseconds; 0 is a special value that indicates that a frame is not part of the animation at all.
  (For a frame to be displayed as briefly as possible, use 1 instead; the library follows this convention when loading
  an image.)
  If this value is missing, or if it is shorter than the number of frames in the image, remaining frames are assumed
  to have zero duration.
- [`PLUM_METADATA_FRAME_DISPOSAL`][metadata-constants]: array of `uint8_t` values containing the disposal methods for
  each frame, i.e., the actions that will be taken to remove the frame from the canvas once its duration expires.
  Possible values are any of the [disposal constants][disposals].
  If this value is missing, or if it is shorter than the number of frames in the image, remaining frames are assumed
  to have a disposal method of [`PLUM_DISPOSAL_NONE`][disposals].

Note that image file formats can impose limitations on the values of these parameters.
The library will approximate them as best as possible when generating an image file.

The sample program for this chapter will create a slideshow out of a list of image files.
As it is significantly longer than the examples shown before, this sample is located [in a separate file][slideshow].

## 8. Color and palette conversions

TBD

## 9. Generating images from scratch

TBD

## 10. Memory management

TBD

## 11. Accessing images not in files

TBD

## 12. Further resources

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
[disposals]: constants.md#frame-disposal-methods
[errors]: constants.md#errors
[feature-macros]: macros.md#feature-test-macros
[find-metadata]: functions.md#plum_find_metadata
[format-constants]: constants.md#image-types
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[load]: functions.md#plum_load_image
[loading-flags]: constants.md#loading-flags
[metadata]: metadata.md
[metadata-constants]: constants.md#metadata-node-types
[metadata-struct]: structs.md#plum_metadata
[methods]: methods.md
[pixel-array]: macros.md#array-declaration
[pixel-casts]: macros.md#array-casts
[pixel-index]: macros.md#pixel-index-macros
[reduce]: functions.md#plum_reduce_palette
[slideshow]: slideshow.c
[sort]: functions.md#plum_sort_palette
[store]: functions.md#plum_store_image
[unprefixed]: macros.md#unprefixed-macros
[validate]: functions.md#plum_validate_image
