# Macros

The library defines a number of macros to make it easier to use.
While using these macros isn't necessary to use the library, they are designed to make some simple operations easier
to write.

- [Color macros](#color-macros)
- [Pixel index macros](#pixel-index-macros)
- [Pixel array macros](#pixel-array-macros)
    - [Array declaration](#array-declaration)
    - [Array type](#array-type)
    - [Array casts](#array-casts)
- [Feature-test macros](#feature-test-macros)
- [Unprefixed macros](#unprefixed-macros)

## Color macros

These macros create a color value from its components (red, green, blue, alpha) and vice-versa.
There are separate macros for each color format, since the bit positions are different; for more information, see the
[Color formats][color-formats] page.

These macros all take numeric values as arguments and result in a value of the same type as the corresponding color
values (`uint16_t`, `uint32_t` or `uint64_t`).
The results are unspecified if the arguments are not in the correct range for the chosen color format.

These macros are also available as [unprefixed macros](#unprefixed-macros) for brevity.

All of these macros evaluate their arguments once and only once.

- `PLUM_COLOR_VALUE_32(red, green, blue, alpha)`, `PLUM_COLOR_VALUE_64(red, green, blue, alpha)`,
  `PLUM_COLOR_VALUE_16(red, green, blue, alpha)`, `PLUM_COLOR_VALUE_32X(red, green, blue, alpha)`: generate a color
  value from its four components.
  For example, in the `PLUM_COLOR_32` color format, `PLUM_COLOR_VALUE_32(255, 255, 0, 0)` will return a color value
  equivalent to solid yellow.
  (This would be fully transparent yellow in the `PLUM_COLOR_32 | PLUM_ALPHA_INVERT` color format.)
- `PLUM_RED_32(color)`, `PLUM_RED_64(color)`, `PLUM_RED_16(color)`, `PLUM_RED_32X(color)`: extract the red component
  of a color.
  For example, `PLUM_RED_16(0x03ff)` would return 31.
  (`0x03ff` is solid yellow in the `PLUM_COLOR_16` color format.)
- `PLUM_GREEN_32(color)`, `PLUM_GREEN_64(color)`, `PLUM_GREEN_16(color)`, `PLUM_GREEN_32X(color)`: extract the green
  component of a color.
- `PLUM_BLUE_32(color)`, `PLUM_BLUE_64(color)`, `PLUM_BLUE_16(color)`, `PLUM_BLUE_32X(color)`: extract the blue
  component of a color.
- `PLUM_ALPHA_32(color)`, `PLUM_ALPHA_64(color)`, `PLUM_ALPHA_16(color)`, `PLUM_ALPHA_32X(color)`: extract the alpha
  component of a color.
  Note that these macros work regardless of whether the color format uses the `PLUM_ALPHA_INVERT` flag, but the
  meaning of the alpha value will naturally inverted.
  For example, solid black is just `0` in the `PLUM_COLOR_32X` color format, but it is `0xc0000000` in the
  `PLUM_COLOR_32X | PLUM_ALPHA_INVERT` color format.
  The `PLUM_ALPHA_32X` macro will naturally extract the component from both values, but they will be inverted:
  `PLUM_ALPHA_32X(0)` is 0, but `PLUM_ALPHA_32X(0xc0000000)` is 3.

## Pixel index macros

These macros calculate the index needed to access an image's pixel data based on its coordinates (X, Y, frame number).
This allows accessing the data directly instead of having to compute indexes manually. For more information, see the
[Accessing pixel and color data][accessing] section.

These macros are also available as [unprefixed macros](#unprefixed-macros) for brevity.

These macros take a single [`struct plum_image *`][image] argument and three numeric coordinate arguments.
All of these macros evaluate their coordinate arguments once and only once; the `image` argument may be evaluated
multiple times.

- `PLUM_PIXEL_INDEX(image, col, row, frame)`: computes the pixel index for the pixel in the image at the indicated
  column (X coordinate), row (Y coordinate) and frame.
  This macro does _not_ check the validity of those values; it will simply compute
  `(frame * height + row) * width + col`.
  This macro results in a `size_t` value.
- `PLUM_PIXEL_8(image, col, row, frame)`, `PLUM_PIXEL_16(image, col, row, frame)`,
  `PLUM_PIXEL_32(image, col, row, frame)`, `PLUM_PIXEL_64(image, col, row, frame)`: expand to the pixel value at the
  specified coordinates.
  These macros expand to an lvalue, meaning that it can be assigned to and its address can be taken.
  The value is of the correct integer type for the chosen macro (`uint8_t`, `uint16_t`, `uint32_t` or `uint64_t`).
  The macro that suits the image's [color format][colors] must be used to access the image's pixels; if the image uses
  [indexed-color mode][indexed], the 8-bit version is the correct one.

## Pixel array macros

These macros allow accessing an image's pixel data as if it was a three-dimensional array directly, indexed by frame,
Y coordinate and X coordinate.
For instance, if `pixels` was declared as such an array, `pixels[frame][row][col]` would address a single pixel.

These macros rely on variable-length array (VLA) support from the compiler, and therefore are only available in C99+
mode and when the compiler has support for VLAs.
While the macros themselves don't declare VLAs, they do declare pointers to VLA types, and therefore will not function
under compilers that only accept constants as array dimensions.
Support for these macros can be tested by checking the [`PLUM_VLA_SUPPORT` feature-test macro](#feature-test-macros).

These macros are also available as [unprefixed macros](#unprefixed-macros) for brevity.

All of these macros take a single [`struct plum_image *`][image] argument.
The `PLUM_PIXEL_ARRAY` macro also takes a declarator argument; this can be any valid C declarator, but it is typically
just a variable name, which will be declared by the macro.
These macros may evaluate their `image` argument multiple times.

### Array declaration

Syntax: `PLUM_PIXEL_ARRAY(declarator, image)`

This macro expands to a pointer-to-array declarator, `declarator`, that declares a pointer to an array of pixels of
the right dimensions for the image `image`.
(Note that a _declarator_ is the part of the declaration that contains the variable name and modifiers.
The _declaration specifiers_, which precede the declarator, contain the type, storage class and qualifiers.
Therefore, a type must be used with this macro: that will be the integer type used to
[access that image's pixel data][accessing]).

For example, the following declaration:

```c
uint32_t PLUM_PIXEL_ARRAY(pixels, image) = image -> data;
```

...will actually declare the following (removing all excess parentheses from the macro):

``` c
uint32_t (* pixels)[image -> height][image -> width] = image -> data;
```

This new array can be used to read and write pixels normally.
For instance, `pixels[0][4][9]` will access the pixel at X = 9, Y = 4 in the image's first frame.

It is important to notice that, in this example, `pixels` is _not_ a variable-length array.
It is indeed just a pointer, and therefore can be assigned the value of `image -> data` (which is of type `void *`).
However, since it is a pointer to an array of variable dimensions, it requires VLA support in the compiler.
All this pointer does is embed in its type the calculation required to access each pixel from its coordinates, so that
the compiler will automatically perform this calculation whenever a pixel is accessed.

### Array type

Syntax: `PLUM_PIXEL_ARRAY_TYPE(image)`

Where [`PLUM_PIXEL_ARRAY`](#array-declaration) expands to a declarator, `PLUM_PIXEL_ARRAY_TYPE` expands to what is
called an _abstract declarator_: what is added after declaration specifiers to name just a type.
This is used to name the type of the pointer-to-array that would be declared, but without declaring anything: for
example, it can be used to cast a value.

The example above could have been written like this, if someone preferred to use explicit casts from `void *`:

``` c
uint32_t PLUM_PIXEL_ARRAY(pixels, image) = (uint32_t PLUM_PIXEL_ARRAY_TYPE(image)) image -> data;
```

In that case, `uint32_t PLUM_PIXEL_ARRAY_TYPE(image)` is the type of the declared pointer.
After removing excess parentheses, it expands to `uint32_t (*)[image -> height][image -> width]`.

### Array casts

In some cases, it is convenient to access pixel data as an array without actually declaring a new pointer variable to
do so.
The following macros can be used for that purpose:

- `PLUM_PIXELS_8(image)`: expands to `((uint8_t PLUM_PIXEL_ARRAY_TYPE(image)) (image) -> data)`, allowing access as
  a three-dimensional array of `uint8_t` values.
- `PLUM_PIXELS_16(image)`: same as above for `uint16_t` values.
- `PLUM_PIXELS_32(image)`: same as above for `uint32_t` values.
- `PLUM_PIXELS_64(image)`: same as above for `uint64_t` values.

For instance, following the examples above, `PLUM_PIXELS_32(image)[0][4][9]` would also expand to the pixel at X = 9,
Y = 4 in the first frame of the image, assuming the image uses `uint32_t` pixel data.
Note that, since this is an ordinary array access, it can be used in an assignment to write to the image's pixels.

## Feature-test macros

Feature-test macros are used to check whether a certain feature has been enabled or not in the header.

Some header features are only enabled if an appropriate version of the language is detected; this prevents compilers
that only support older versions of the language from failing to compile due to using unsupported features.

Some of these features are instead used by the library user to enable or disable some features in the header.
Those macros must be defined _before_ the library header is included for the first time.

The following macros are defined by the library, and can be used for conditional compilation (e.g., in an `#if`
directive):

- `PLUM_VLA_SUPPORT`: expands to a non-zero value if [VLA-based macros](#pixel-array-macros) are available, or zero
  otherwise.
- `PLUM_ANON_MEMBERS`: expands to a non-zero value if anonymous unions are available in structs (in which case the
  [`plum_image`][image] struct will have its anonymous unions, as shown in the definition), or to zero otherwise (in
  which case that struct will only contain the `palette` and `data` members instead of their respective unions).
- `PLUM_HEADER`: defined when the library header has been included.
  Used by the header itself in its `#ifdef` include guards.

The following macros can be defined by the user (before including the library header) to toggle some of its features:

- `PLUM_UNPREFIXED_MACROS`: enables the use of [unprefixed macros](#unprefixed-macros), which are short forms of the
  macros described in the previous sections, added for readability.
  Disabled by default (since those macros don't follow the `PLUM_` prefix [convention][conventions]); defining this
  macro will enable them.
- `PLUM_NO_STDINT`: indicates that the `<stdint.h>` header isn't available; this macro is provided for C89
  compatibility.
  By default, the library header will `#include <stdint.h>`; defining this macro will prevent it from doing that.
  If this macro is defined, the user **must** provide their own `typedef` definitions for the `uint8_t`, `uint16_t`,
  `uint32_t` and `uint64_t` types _before_ including the library header; otherwise, compilation will fail.

Note that the library will internally use some additional feature-test macros to function properly; all of these
macros are properly prefixed with `PLUM_` to avoid colliding with user-defined macros.
These macros are intentionally undocumented, and may change from one release to the next; they are not intended for
end users of the library.

## Unprefixed macros

Most of the macros defined above have a name that is too long for practical use.
The names were chosen to be unique and clear, but at the same time, they aren't practical in most expressions.

While users can define their own shorter-named macros that expand to the ones above, some common short forms are
already provided by the library header.
Since these unprefixed names violate the `PLUM_` prefix [convention][conventions], they are disabled by default and
they must be enabled by defining the [`PLUM_UNPREFIXED_MACROS` feature-test macro](#feature-test-macros) before
including the library header.

All of these macros are equivalent to one of the macros defined above.

- `COLOR32`, `COLOR64`, `COLOR16`, `COLOR32X`: equivalent to [`PLUM_COLOR_VALUE_32`](#color-macros) and the like.
- `RED32`, `RED64`, `RED16`, `RED32X`, `GREEN32`, `GREEN64`, `GREEN16`, `GREEN32X`,  `BLUE32`, `BLUE64`, `BLUE16`,
  `BLUE32X`, `ALPHA32`, `ALPHA64`, `ALPHA16`, `ALPHA32X`: equivalent to [`PLUM_RED_32`](#color-macros) and the like
  for each combination of component and [color format][color-formats].
- `PIXEL`: equivalent to [`PLUM_PIXEL_INDEX`](#pixel-index-macros).
- `PIXEL8`, `PIXEL16`, `PIXEL32`, `PIXEL64`: equivalent to [`PLUM_PIXEL_8`](#pixel-index-macros) and the like.
- `PIXARRAY`: equivalent to [`PLUM_PIXEL_ARRAY`](#array-declaration).
  Available only when [`PLUM_VLA_SUPPORT`](#feature-test-macros) is non-zero.
- `PIXARRAY_T`: equivañent to [`PLUM_PIXEL_ARRAY_TYPE`](#array-type).
  Available only when [`PLUM_VLA_SUPPORT`](#feature-test-macros) is non-zero.
- `PIXELS8`, `PIXELS16`, `PIXELS32`, `PIXELS64`: equivalent to [`PLUM_PIXELS_8`](#array-casts) and the like.
  Available only when [`PLUM_VLA_SUPPORT`](#feature-test-macros) is non-zero.

Note that these short unprefixed macros can be used exactly where the originals would be.
For example, the following snippet is valid:

``` c
// get the bottom right pixel's color...
uint32_t color = PIXEL32(image, image -> width - 1, image -> height - 1, 0);
// and set the top left pixel to the same color, RGB-rotated
PIXEL32(image, 0, 0, 0) = COLOR32(GREEN32(color), BLUE32(color), RED32(color), ALPHA32(color));
```

* * *

Prev: [Constants and enumerations](constants.md)

Next: [Alphabetical declaration list](alpha.md)

Up: [README](README.md)

[accessing]: colors.md#accessing-pixel-and-color-data
[color-formats]: colors.md#formats
[colors]: colors.md
[conventions]: conventions.md#conventions
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
