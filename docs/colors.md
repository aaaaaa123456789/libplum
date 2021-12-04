# Color formats

- [Formats](#formats)
- [Indexed-color mode](#indexed-color-mode)
- [Accessing pixel and color data](#accessing-pixel-and-color-data)

## Formats

Regardless of the actual formats used for color and pixel data in image files, the library uses a number of standard
color formats and converts to and from those formats respectively when loading and generating image files.
The user chooses which color format to use via an argument when loading an image (or a struct member when creating one
manually); therefore, while the library supports processing data in all supported color formats, users only need to
choose one and use the one they prefer.

All color formats contain red, green, blue and alpha channels.
There is no dedicated support for grayscale or no transparency formats; images using those formats will be converted
to RGBA when loaded.

All color formats use fixed-width integers for color values.
The channels are always in RGBA order from least significant to most significant bit; in other words, the red channel
always takes up the least significant bits and the alpha channel always takes up the most significant bits.
There are no unused bits, and the three color channels always have the same bit width.

There are [helper macros][macros] available to generate color values out of their components and extract those
components from a color value.
However, since the color format is fully defined here, it is also possible to use bit masks and shifts directly.

The full color format is defined by the color format constant and the alpha inversion flag.
For example, `PLUM_COLOR_32` and `PLUM_COLOR_64 | PLUM_ALPHA_INVERT` are examples of color formats.
The `PLUM_COLOR_MASK` constant can be used to mask the color format constant out of a flags value; `PLUM_ALPHA_INVERT`
is a bitflag of its own and therefore acts as its own mask. Masking with `PLUM_COLOR_MASK | PLUM_ALPHA_INVERT` will
extract the full color format out of a flags value.

The color format of an image (i.e., a [`struct plum_image`][image]) is given by its `color_format` member.
The library will always mask this value against the `PLUM_COLOR_MASK | PLUM_ALPHA_INVERT` mask, thus ignoring all
other bits.
The color format of an image loaded through the [`plum_load_image`][load] function is determined by the `flags`
argument to that function; the color format bits will be copied from that argument to the image.
It can be noticed that the `flags` argument is of type `unsigned`, while the `color_format` member of the image struct
is `uint8_t`; this is because there are other flags in the higher bits that don't affect the color format and
therefore aren't copied to the image struct.

The color formats themselves, defined by their constants, are:

|    Constant    |Underlying type|RGB bit width|Alpha bit width|
|:---------------|:-------------:|:-----------:|:-------------:|
|`PLUM_COLOR_16` |  `uint16_t`   |      5      |       1       |
|`PLUM_COLOR_32` |  `uint32_t`   |      8      |       8       |
|`PLUM_COLOR_32X`|  `uint32_t`   |     10      |       2       |
|`PLUM_COLOR_64` |  `uint64_t`   |     16      |      16       |

Therefore, the bit masks for each component are:

|     Format     |      Red mask      |     Green mask     |      Blue mask     |     Alpha mask     |
|:--------------:|-------------------:|-------------------:|-------------------:|-------------------:|
|`PLUM_COLOR_16` |            `0x001f`|            `0x03e0`|            `0x7c00`|            `0x8000`|
|`PLUM_COLOR_32` |        `0x000000ff`|        `0x0000ff00`|        `0x00ff0000`|        `0xff000000`|
|`PLUM_COLOR_32X`|        `0x000003ff`|        `0x000ffc00`|        `0x3ff00000`|        `0xc0000000`|
|`PLUM_COLOR_64` |`0x000000000000ffff`|`0x00000000ffff0000`|`0x0000ffff00000000`|`0xffff000000000000`|

Since most applications don't need transparency, in order to make those applications simpler, alpha isn't stored in
the conventional way, but rather, it is negated.
In other words, by default, **zero alpha means a color is fully opaque, and maximum alpha means a color is fully
transparent**.
Therefore, calculating a color by ORing its red, green and blue components will result in an opaque color, instead of
a fully-transparent color by accident.

Since applications that do use transparency might prefer to use the alpha channel the conventional way (i.e., where
zero alpha means transparent and max alpha means opaque), the `PLUM_ALPHA_INVERT` flag is provided.
When this flag is set, the alpha channel is inverted, thus converting it to its conventional representation.
For instance, in the `PLUM_COLOR_32` format, solid yellow is `0x0000ffff` (max red and green, no blue or alpha); in
the `PLUM_COLOR_32 | PLUM_ALPHA_INVERT` format, solid yellow is `0xff00ffff` (now with max alpha as well).

## Indexed-color mode

The library fully supports indexed-color mode, i.e., images where colors are defined by a palette and pixels reference
indexes into that palette.

In this case, image data is stored as `uint8_t` values; those values are indexes into the palette.
The palette itself stores colors in whichever color format is set for the image (as determined by its `color_format`
member).

An image uses indexed-color mode if its `palette` member (and therefore, the corresponding `palette16`, `palette32`
and `palette64` members, which alias `palette`) is not null.
In this case, the `max_palette_index` member determines the maximum valid index value (and thus implicitly the size of
the palette); note that this maximum is _inclusive_, so that a `max_palette_index` of 3 indicates a palette with four
colors (with indexes ranging between 0 and 3).
A palette can have up to 256 colors.

Indexed-color mode is always opt-in.
New images created with the [`plum_new_image`][new] function are zero-initialized, and therefore have a null palette
(which means they don't use indexed-color mode); a palette must be manually loaded to use indexed-color mode.
Images loaded through [`plum_load_image`][load] will only use indexed-color mode if it is requested in the `flags`
argument; otherwise (and by default), the library will remove the palette and convert it to direct-color mode (i.e.,
the traditional mode where pixels contain colors directly) when loading.
Naturally, statically-initialized images will contain whatever data is assigned to them, and images created through
[`plum_copy_image`][copy] will use the same mode and format as the image they are copying.

## Accessing pixel and color data

Regular images (i.e., images in direct-color mode) will contain their pixel data as an array of `uint16_t`, `uint32_t`
or `uint64_t` values depending on the color format they are using.
This data must always be accessed through a pointer of the correct type; failure to do so will result in the usual
errors that derive from accessing a buffer through a pointer of a different type, up to undefined behavior.
The [`plum_image` struct][image] contains a `void * data` member that points to this data; in C99+ and C++ modes,
there are also `data16`, `data32` and `data64` aliases of the corresponding integer types to avoid the need to cast
this pointer member.
Note that both `PLUM_COLOR_32` and `PLUM_COLOR_32X` color data is accessed through `uint32_t` values, and therefore
both can use the `data32` member.
(The distinction is only made in macros that handle the color components, not in struct members that only differ in
their data types.)

Indexed-color mode images will contain their pixel data as an array of `uint8_t` values; these values are indexes
into the palette, and they must all be less than or equal to the image's `max_palette_index` member.
(Images created through [`plum_load_image`][load] will always fulfill this restriction.)
In C99+ and C++ modes, there is a `data8` alias for the `data` member that allows this 8-bit access without a cast.
The palette itself is accessed through the `void * palette` member; much like the image data, in C99+ and C++ modes it
has `palette16`, `palette32` and `palette64` aliases to access it without a cast.
The palette uses the color format defined for the image and is subject to the same constraints as the image data in
direct-color mode.
For images created through [`plum_load_image`][load] or [`plum_copy_image`][copy], the palette is only guaranteed to
contain `max_palette_index + 1` elements; it is **not** required to contain 256 elements unless `max_palette_index` is
255.

In all cases, the image data is conceptually a three-dimensional array of values of the corresponding integer type:
frame-major, then row-major.
For example, if a 3-frame image has a width of 10 and a height of 20, entry 1 in the array will correspond to X = 1,
Y = 0 in the first frame, entry 10 will be X = 0, Y = 1, and entry 200 will be the top left corner of the next frame.
(Note that, in C99+ mode if the compiler has VLA support, there is [a macro][vla] that allows redefining this pointer
as a true three-dimensional array to allow accessing it using the three coordinates directly.
Also, in C++ mode, there are [helper accessor methods][methods] that take the three coordinates directly and return a
reference to the corresponding pixel data.)

* * *

Prev: [Conventions and limitations](conventions.md)

Next: [Data structures](structs.md)

Up: [README](README.md)

[copy]: #
[image]: structs.md#plum_image
[load]: #
[macros]: #
[methods]: #
[new]: #
[vla]: #
