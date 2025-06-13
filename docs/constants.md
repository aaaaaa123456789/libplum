# Constants and enumerations

This page lists all constants defined by the library, including constants that are part of `enum` types.
These constants are _not_ meant to be used in preprocessor directives like `#if`.
An alphabetical list (with links) is provided [in a separate page][alphabetical].

Some `enum` types contain an extra constant at the end indicating the number of constants they define, prefixed with
`PLUM_NUM`.
(For example, the [`PLUM_NUM_IMAGE_TYPES`](#number-of-constants) constant corresponds to the number of image types
defined by the [`enum plum_image_types`](#image-types) type, and is one greater than the highest image type constant
in that `enum`.)
Those constants will be listed separately, in the [Number of constants](#number-of-constants) section.

All `enum` types contain values designated as "zero".
These constants have a value of zero; zero values always have special semantics, often different from the remaining
values in the `enum`.

- [Special loading and storing modes](#special-loading-and-storing-modes)
- [Image types](#image-types)
- [Metadata node types](#metadata-node-types)
- [Frame disposal methods](#frame-disposal-methods)
- [Loading flags](#loading-flags)
- [Errors](#errors)
- [Number of constants](#number-of-constants)
- [Color mask constants](#color-mask-constants)

## Special loading and storing modes

These constants are used by the [`plum_load_image`][load] and [`plum_store_image`][store] functions to indicate that
they should use a special [loading or storing mode][loading-modes] instead of reading from or writing to a memory
buffer.

These constants are of `size_t` type, and they don't belong to an `enum`.
Their values are the highest possible values of that type, and therefore they are extremely unlikely to collide with
actual buffer sizes.

- `PLUM_MODE_FILENAME`: indicates that the `buffer` argument is a `char *` value containing a filename.
- `PLUM_MODE_BUFFER`: indicates that the `buffer` argument is a [`struct plum_buffer *`][buffer] value that describes
  (or will describe) a memory buffer and its size.
- `PLUM_MODE_CALLBACK`: indicates that the `buffer` argument is a [`struct plum_callback *`][callback] value,
  containing a callback function that will be called to read or write data.

Additionally, the `PLUM_MAX_MEMORY_SIZE` constant represents the maximum size that will be interpreted as a true size
instead of a special loading/storing mode.
(A value of `size_mode` no larger than this constant will be treated as the size of the buffer pointed to by
`buffer`.)

For more information, see the [Loading and storing modes][loading-modes] page.

## Image types

**Type:** `enum plum_image_types`

These constants are used for the `type` member of a [`struct plum_image`][image], to indicate the file format the
image used (when loading it) or will use (when storing it).

- `PLUM_IMAGE_NONE` (zero): no image type.
  Used by an image for which an image type hasn't been specified, such as one just created by [`plum_new_image`][new].
  It may also be used as an explicit "no type" designator.
- `PLUM_IMAGE_BMP`: BMP (Windows Bitmap) file.
- `PLUM_IMAGE_GIF`: GIF (CompuServe's Graphics Interchange Format) format, including static images and animations.
- `PLUM_IMAGE_PNG`: PNG (Portable Network Graphics) file.
- `PLUM_IMAGE_APNG`: animated PNG file.
  Treated as a separate format because it is not actually compatible with PNG (due to using ancillary chunks to store
  critical animation information), which will cause APNG-unaware viewers and editors to handle it incorrectly.
- `PLUM_IMAGE_JPEG`: JPEG (Joint Photographers Expert Group) file.
- `PLUM_IMAGE_PNM`: netpbm's PNM (Portable Anymap) format.
  When loading, it represents any possible PNM file; however, only PPM and PAM files will be written.
- `PLUM_IMAGE_QOI`: QOI (Quite OK Image) file.

For more information, see the [Supported file formats][formats] page.

## Metadata node types

**Type:** `enum plum_metadata_types`

These constants are used to specify the type of a metadata node.

- `PLUM_METADATA_NONE` (zero): unused node.
  This constant represents a node that doesn't contain any useful data; it is the only metadata type defined by the
  library (i.e., not negative) that can appear more than once in an image.
- `PLUM_METADATA_COLOR_DEPTH`: node containing bit depth information for each channel in the image.
- `PLUM_METADATA_BACKGROUND`: node containing a background color for the image.
- `PLUM_METADATA_LOOP_COUNT`: node used for animations, indicating how many times they will repeat.
- `PLUM_METADATA_FRAME_DURATION`: node containing the duration of each frame in an animation.
- `PLUM_METADATA_FRAME_DISPOSAL`: node indicating the action that must be taken on an animation's frame buffer after
  an animation frame has been rendered and displayed for the required amount of time.
- `PLUM_METADATA_FRAME_AREA`: node containing the true dimensions and coordinates of the frames that comprise a
  multi-frame file.

For more information, see the [Metadata][metadata] page.

## Frame disposal methods

**Type:** `enum plum_frame_disposal_methods`

These constants are used to specify the actions that will be taken when replacing one animation frame with the next:
both when replacing the framebuffer's contents with the frame in question, and after the frame has been rendered for
the required length of time.

Not all animation file formats support all actions listed here; if some aren't supported, the best available
equivalent will be used when writing out the image data.

- `PLUM_DISPOSAL_NONE` (zero): default action: do nothing.
- `PLUM_DISPOSAL_BACKGROUND`: once the frame has been displayed, replace it with background-color pixels.
- `PLUM_DISPOSAL_PREVIOUS`: once the frame has been displayed, revert its pixels to their previous state.
- `PLUM_DISPOSAL_REPLACE`: when displaying the current frame, after disposing of the last frame's pixels (if any),
  replace the framebuffer's pixels with this frame's pixels (instead of merging them according to transparency).
- `PLUM_DISPOSAL_BACKGROUND_REPLACE`: `PLUM_DISPOSAL_REPLACE` plus `PLUM_DISPOSAL_BACKGROUND`.
- `PLUM_DISPOSAL_PREVIOUS_REPLACE`: `PLUM_DISPOSAL_REPLACE` plus `PLUM_DISPOSAL_PREVIOUS`.

Note that it is possible to construct the last two constants by addition: `PLUM_DISPOSAL_BACKGROUND_REPLACE` is equal
to `PLUM_DISPOSAL_BACKGROUND + PLUM_DISPOSAL_REPLACE`, and likewise for `PLUM_DISPOSAL_PREVIOUS_REPLACE`.

For more information, see the description of the [`PLUM_METADATA_FRAME_DISPOSAL` metadata node][disposal].

## Loading flags

These constants are of `unsigned long` type, and they don't belong to an `enum`.

These flags are used by the [`plum_load_image`][load] function to specify various parameters regarding how the image
will be loaded.

[Color format][colors] flags, which are also part of these flags (since loading an image requires specifying which
color format will be used to represent it after loading), are also used by [the image itself][image] (in its
`color_format` member) and by functions that take a color format as an argument.
The palette-sorting flags are likewise also used by palette-sorting functions.

The flags are grouped by their purpose; all groups (except for bit flags) also have a "zero" member.
Naturally, when flags are combined with a bitwise OR (`|`), the "zero" member (which has a value of 0) will be the
default and can be left out.
Groups also have a mask member, which can be used to mask out that group's bits (for checking, etc.)

Note that, since these constants are meant to be combined via bitwise OR (`|`), their values are **not** consecutive.

**Color format flags:** these flags are used to determine the image's or the color buffer's color format.
See the [Color formats][colors] page for more information.

- `PLUM_COLOR_32` (zero): `uint32_t` colors, RGBA 8.8.8.8.
- `PLUM_COLOR_64`: `uint64_t` colors. RGBA 16.16.16.16.
- `PLUM_COLOR_16`: `uint16_t` colors, RGBA 5.5.5.1.
- `PLUM_COLOR_32X`: `uint32_t` colors, RGBA 10.10.10.2.
- `PLUM_COLOR_MASK`: bit mask that can be used to extract this bitfield.
- `PLUM_ALPHA_INVERT`: additional bit flag that can be ORed to indicate that zero alpha means fully transparent, not
  fully opaque.
  This bit is _not_ part of the `PLUM_COLOR_MASK` mask, and therefore must be ORed into that mask too to extract the
  full color format out of a flags value.

**Indexed-color mode flags:** these flags are used to control whether the loaded image will use
[indexed-color mode][indexed] or not.
Some modes will load the image in direct-color mode (i.e., without a palette) or indexed-color mode (i.e., with a
palette) depending on the image data; in those cases, the image's `palette` member can be used to determine what mode
the image is using (null pointer for direct-color mode or non-null for indexed-color mode).

- `PLUM_PALETTE_NONE` (zero): don't use indexed-color mode at all.
  If the image has a palette, it will be removed on load.
  This is the default because it allows users to be unaware of palettes if they don't need the functionality.
- `PLUM_PALETTE_LOAD`: load a palette if the image has one.
  This mode preserves the original image faithfully: it will neither create nor remove a palette.
- `PLUM_PALETTE_GENERATE`: load a palette if the image has one, or try to generate one otherwise.
  This mode indicates that the user prefers to use indexed-color mode, but will still load an image without a palette
  if the image has too many colors.
  It will also adjust the image's `max_palette_index` member to match the number of colors actually used by the image.
- `PLUM_PALETTE_FORCE`: always load or generate a palette.
  If the image doesn't have a palette and generating one isn't possible, loading fails.
  This mode indicates that the user only supports indexed-color mode; it will never load an image without a palette.
  It will also adjust the image's `max_palette_index` member, like `PLUM_PALETTE_GENERATE` does.
- `PLUM_PALETTE_MASK`: bit mask that can be used to extract this bitfield.

**Palette-sorting flags:** these flags are used to control the way palettes are sorted when one is generated.
They are also used by [`plum_convert_colors_to_indexes`][convert-colors] and [`plum_sort_palette`][sort-palette] to
determine how the colors will be sorted.

- `PLUM_SORT_LIGHT_FIRST` (zero): sort colors from brightest to darkest.
- `PLUM_SORT_DARK_FIRST`: sort colors from darkest to brightest.
  This value also functions as a bit mask for this bit field, since it is the only non-zero value it can have.

**Additional bit flags:** these flags represent additional operations that will be carried out when an image is
loaded.
These flags are all bit flags; therefore, they are all their own bit masks and none of them is zero.
Leave them out to not enable the operations they request.

- `PLUM_ALPHA_REMOVE`: indicates that the alpha channel must be removed when the image is loaded, converting all
  colors to fully opaque.
- `PLUM_SORT_EXISTING`: indicates that, if the image has a palette and that palette is loaded, that palette must be
  sorted.
  (By default, only generated palettes are sorted.)
- `PLUM_PALETTE_REDUCE`: indicates that, if the image has a palette, that palette should be reduced to a minimum
  palette by removing unused and duplicate colors.

## Errors

**Type:** `enum plum_errors`

These constants represent error values.
Error values are used by several library functions to indicate the reason why a function failed.

The special constant `PLUM_OK` is also part of this `enum`, but it has a value of zero and it indicates success.
(All error constants have positive values.)
This ensures that errors can be easily detected by checking whether a result value is zero or not.

Some functions return negated error constants on failure.
(For instance, if a `PLUM_ERR_INVALID_ARGUMENTS` error occurs, those functions will return
`-PLUM_ERR_INVALID_ARGUMENTS`.)
Since all error constants (i.e., all constants listed other than `PLUM_OK`) are positive, all such values will become
negative.

The descriptions listed here are the same as the messages returned by [`plum_get_error_text`][error-text].

- `PLUM_OK`: success
- `PLUM_ERR_INVALID_ARGUMENTS`: invalid argument for function
- `PLUM_ERR_INVALID_FILE_FORMAT`: invalid image data or unknown format
- `PLUM_ERR_INVALID_METADATA`: invalid image metadata
- `PLUM_ERR_INVALID_COLOR_INDEX`: invalid palette index
- `PLUM_ERR_TOO_MANY_COLORS`: too many colors in image
- `PLUM_ERR_UNDEFINED_PALETTE`: image palette not defined
- `PLUM_ERR_IMAGE_TOO_LARGE`: image dimensions too large
- `PLUM_ERR_NO_DATA`: image contains no image data
- `PLUM_ERR_NO_MULTI_FRAME`: multiple frames not supported
- `PLUM_ERR_FILE_INACCESSIBLE`: could not access file
- `PLUM_ERR_FILE_ERROR`: file input/output error
- `PLUM_ERR_OUT_OF_MEMORY`: out of memory

## Number of constants

These constants represent the number of values in some of the `enum` types listed above.
They are therefore one greater than the highest constant of that type.

- `PLUM_NUM_IMAGE_TYPES`: number of [image types](#image-types)
- `PLUM_NUM_METADATA_TYPES`: number of [metadata node types](#metadata-node-types)
- `PLUM_NUM_DISPOSAL_METHODS`: number of [frame disposal methods](#frame-disposal-methods)
- `PLUM_NUM_ERRORS`: number of [error constants](#errors)

## Color mask constants

These constants represent the bit masks needed to extract each component (red, green, blue, alpha) from a color value.
Naturally, they are also the maximum value for the corresponding components.

There are separate constants for each [color format][color-formats], since the bit positions for each format are
different.

These constants don't belong to an `enum`, and they are all of the same type as the corresponding color values
(`uint16_t`, `uint32_t` and `uint64_t`).

- `PLUM_RED_MASK_32`, `PLUM_RED_MASK_64`, `PLUM_RED_MASK_16`, `PLUM_RED_MASK_32X`: red component bit mask.
- `PLUM_GREEN_MASK_32`, `PLUM_GREEN_MASK_64`, `PLUM_GREEN_MASK_16`, `PLUM_GREEN_MASK_32X`: green component bit mask.
- `PLUM_BLUE_MASK_32`, `PLUM_BLUE_MASK_64`, `PLUM_BLUE_MASK_16`, `PLUM_BLUE_MASK_32X`: blue component bit mask.
- `PLUM_ALPHA_MASK_32`, `PLUM_ALPHA_MASK_64`, `PLUM_ALPHA_MASK_16`, `PLUM_ALPHA_MASK_32X`: alpha component bit mask.

* * *

Prev: [Function reference](functions.md)

Next: [Macros](macros.md)

Up: [README](README.md)

[alphabetical]: alpha.md
[buffer]: structs.md#plum_buffer
[callback]: structs.md#plum_callback
[color-formats]: colors.md#formats
[colors]: colors.md
[convert-colors]: functions.md#plum_convert_colors_to_indexes
[disposal]: metadata.md#plum_metadata_frame_disposal
[error-text]: functions.md#plum_get_error_text
[formats]: formats.md
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[load]: functions.md#plum_load_image
[loading-modes]: modes.md
[metadata]: metadata.md
[new]: functions.md#plum_new_image
[sort-palette]: functions.md#plum_sort_palette
[store]: functions.md#plum_store_image
