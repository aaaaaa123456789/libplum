# Supported file formats

Each file format supported by the library has its own particularities in its implementation.
While the library attempts to smooth out the differences as much as possible, some minor differences remain, that can
be seen when an image is loaded in one format and stored in another.

All supported formats are documented here, along with their restrictions and some implementation notes.

- [Definitions](#definitions)
- [BMP](#bmp)
- [GIF](#gif)
- PNG
- APNG
- JPEG
- PNM

## Definitions

Some terms are used when describing many of the formats, and they are therefore defined here.

**True bit depth**

Actual bit depth of a component or set of components of an image, as calculated by the library.
It is always a value between 1 and 16 (or between 0 and 16 for the alpha component).
If the image has a [`PLUM_METADATA_COLOR_DEPTH`][metadata-constants] metadata node, then the bit depth for that
component is taken from the information in that node; otherwise, a default is used.

The true bit depth of a set of components is the greatest bit depth amongst those components; in particular, the true
bit depth for the image as a whole is the greatest bit depth amongst all four components (red, green, blue, alpha).
Note that the true bit depth of the whole image does **not** refer to the sum of the true bit depths of every
component: for example, an image using 8 bits for each of the red, green and blue components (and no transparency) has
a true bit depth of 8, not 24.

The exact rules for the true bit depth of a single component are:

- If the metadata node defines a non-zero color depth for that component, that value is used.
- If the component is a color component (i.e., red, green or blue), and the metadata node defines a depth of 0 for all
  three color components, but a non-zero depth for the gray component, the gray component's depth is used.
- In all cases above, if the defined depth is greater than 16, it is clamped to 16.
- If the metadata node defines a color depth of zero for that component, or if there is no color depth metadata node
  at all, then the bit width of that component in the image's [color format][color-formats] is used as a default.
- For the alpha component, if all colors in the image are fully opaque (in the image's palette if the image uses
  [indexed-color mode][indexed], or in the image's pixels otherwise), the bit depth is set to zero, regardless of the
  calculated depth above.

**Empty pixel**

An empty pixel is a pixel that is fully transparent, and whose RGB components match those of the image's background
color (as defined by a [`PLUM_METADATA_BACKGROUND`][metadata-constants] metadata node).
If the image doesn't have a defined background color, then an empty pixel is a fully transparent black pixel.

Empty pixels may be used to determine borders.
A portion of an image is considered to be an empty border if all pixels in that border (i.e., between the boundary of
the border and the edge of the image) are empty pixels.

## BMP

All variants of the Windows Bitmap (BMP) format are supported.
Other variants, such as OS/2 BMP files, will error out with [`PLUM_ERR_INVALID_FILE_FORMAT`][errors]; those variants
are extremely rare and most likely no longer in use.

BMP files only support a single frame; attempting to generate a file with two or more frames will fail with
[`PLUM_ERR_NO_MULTI_FRAME`][errors].
The maximum width and height for an image is `0x7fffffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

Images using [indexed-color mode][indexed] are fully supported, but they cannot contain transparency.
Therefore, if an image uses transparency, the file will be generated without a palette.

If an image has a [true bit width](#definitions) of 8 or less and it doesn't use transparency, the usual RGB888 format
is used for output.
Otherwise, variable bit masks are used, with each component having the width determined by its true bit width.
Variable bit width files are limited to a total of 32 bits per color, so if the sum of the true bit widths for all
components exceeds 32, they are proportionally reduced to fit.

## GIF

GIF (Graphics Interchange Format) files are supported as long as they contain all the palettes used by the image.
Images with implicit or "default" palettes aren't supported and will fail with [`PLUM_ERR_UNDEFINED_PALETTE`][errors].
(Likewise, image files that only define palettes and don't define any image frames will fail with
[`PLUM_ERR_NO_DATA`][errors].)

The maximum width and height for an image is `0xffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

An image will be loaded in [indexed-color mode][indexed] if it has a global palette, it has no per-frame palettes, and
it either uses no transparency or uses the same index for transparency in all frames.
In all other cases, the image will be loaded as if it didn't have a palette.

When generating a file, [indexed-color mode][indexed] images will use a global palette.
All other images will use per-frame palettes, and generating them will fail with [`PLUM_ERR_TOO_MANY_COLORS`][errors]
if a frame uses more than 256 distinct colors.
For an image using per-frame palettes, if any frame of the image has a border made of [empty pixels](#definitions),
that frame will be encoded with reduced dimensions.

The library will ignore the image's version number (i.e., it will parse `GIF87a` and `GIF89a` files identically) and
will always generate `GIF89a` files.

The GIF format doesn't support the [`PLUM_DISPOSAL_REPLACE`][disposals] disposal method, or any of the combinations
including it; that will be converted when generating an image file.

Valid [frame durations][durations] are between 0 and 655.35 seconds, in intervals of 0.01 seconds; values will be
rounded accordingly when generating a file.
When loading a file, a frame with a duration of 0 will be loaded as 1 nanosecond.
When generating a file, a frame duration of 0 has no special meaning and is just converted like any other value.

The common loop count extension is supported, and will be loaded as a [`PLUM_METADATA_LOOP_COUNT`][metadata-constants]
metadata node.
The maximum supported loop count is `0xffff`; higher values will be treated as 0 (i.e., infinity) when generating a
file.
Animations lacking this metadata node are not looped, and the corresponding files will be generated without the loop
count extension; if the loop count is 1, the extension won't be generated either.

* * *

Prev: [C++ helper methods](methods.md)

Up: [README](README.md)

[color-formats]: colors.md#formats
[disposals]: constants.md#frame-disposal-methods
[durations]: metadata.md#plum_metadata_frame_duration
[errors]: constants.md#errors
[indexed]: colors.md#indexed-color-mode
[metadata-constants]: constants.md#metadata-node-types
