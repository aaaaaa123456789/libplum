# Function reference

This page lists all functions exposed by the library's API, their arguments and return values, and their behavior.
The functions are grouped by functionality, but an alphabetical list (with links) is provided at the end.

As it is mentioned in the [Conventions][conventions] page, all functions that take a pointer to `const` as an argument
treat the `const` qualifier as deep.
In other words, the function won't modify any of the data accessible through that pointer.

- [Basic functionality](#basic-functionality)
    - [`plum_new_image`](#plum_new_image)
    - [`plum_copy_image`](#plum_copy_image)
    - [`plum_load_image`](#plum_load_image)
    - [`plum_store_image`](#plum_store_image)
    - `plum_destroy_image`
- Validation
    - `plum_validate_image`
    - `plum_check_valid_image_size`
    - `plum_validate_palette_indexes`
- Color operations
    - `plum_convert_color`
    - `plum_convert_colors`
    - `plum_remove_alpha`
    - `plum_sort_colors`
    - `plum_color_buffer_size`
- Palette operations
    - `plum_convert_colors_to_indexes`
    - `plum_convert_indexes_to_colors`
    - `plum_sort_palette`
    - `plum_get_highest_palette_index`
- Miscellaneous image operations
    - `plum_find_metadata`
    - `plum_rotate_image`
    - `plum_pixel_buffer_size`
    - `plum_palette_buffer_size`
- Memory management
    - `plum_malloc`
    - `plum_calloc`
    - `plum_realloc`
    - `plum_allocate_metadata`
    - `plum_free`
- Library information
    - `plum_get_file_format_name`
    - `plum_get_error_text`
- Alphabetical function index

## Basic functionality

These functions implement the basic functionality of the library: loading and storing image files and creating and
releasing images.
Most simple uses of the library require these functions alone; users are encouraged to familiarize themselves with
them first.

The [`plum_new_image`](#plum_new_image), [`plum_copy_image`](#plum_copy_image) and
[`plum_load_image`](#plum_load_image) functions all serve the purpose of creating an image with some data; therefore,
they are collectively referred to elsewhere in the documentation as the _constructor functions_.
Likewise, the [`plum_destroy_image`](#plum_destroy_image) behaves as the corresponding destructor, releasing all
memory used by an image.
The [`plum_store_image`](#plum_store_image) function implements the other critical library feature: generating an
image file from image data.

### `plum_new_image`

``` c
struct plum_image * plum_new_image(void);
```

**Description:**

This function allocates a new image (i.e., a new [`struct plum_image`][image]), returning a pointer to it.
The image is zero-initialized, except for the `allocator` member, which is initialized appropriately.
This image struct is allocated as if by [`plum_malloc`](#plum_malloc), and will be deallocated by
[`plum_destroy_image`](#plum_destroy_image); see the [Memory management][memory] page for more details.

**Arguments:** none.

**Return value:**

Pointer to the new image, or `NULL` on failure due to memory exhaustion.

### `plum_copy_image`

``` c
struct plum_image * plum_copy_image(const struct plum_image * image);
```

**Description:**

This function creates a copy of an image.
The copy will contain its own copies of the pixel data, the palette data if any, and all the metadata nodes; metadata
nodes will be inserted in the same order as the original.
(However, the `user` member of the [`plum_image`][image] struct will be copied directly, since the library has no way
to know the layout of the user data stored in this member, if any.)

The function can fail if `image` is `NULL`, its `data` member is `NULL`, or there's not enough memory to allocate all
the necessary buffers.

The image created by this function must be deallocated with the [`plum_destroy_image`](#plum_destroy_image) function;
see the [Memory management][memory] page for more details.

**Arguments:**

- `image`: image to be copied.

**Return value:**

Pointer to the copied image, or `NULL` on failure.

### `plum_load_image`

``` c
struct plum_image * plum_load_image(const void * restrict buffer, size_t size,
                                    unsigned flags, unsigned * restrict error);
```

**Description:**

This function, together with [`plum_store_image`](#plum_store_image), implements most of the library's functionality.
This function will load an image, automatically detecting the file format in use, and, if loading is successful,
return a [`plum_image`][image] struct for the application to use.

The image can be loaded from a memory buffer, from a file, from a [`plum_buffer`][buffer] struct, or through a
user-defined callback; see the [Loading and storing modes][loading-modes] page for more information.

The loaded image data will contain all of the image's frames, as well as all relevant nodes of [metadata][metadata].
Depending on the value of the `flags` argument, the image may use [indexed-color mode][indexed] or not; the `palette`
member can be used to determine which mode it uses.
(The `palette` member will be `NULL` if the image doesn't use a palette.)
The image's [color format][colors] (and its `color_format` member) will also be determined by the `flags` argument.

The image struct and the data buffers it will contain will be allocated as if by [`plum_malloc`](#plum_malloc), and
therefore must be released through the [`plum_destroy_image`](#plum_destroy_image) function.
See the [Memory management][memory] page for more details.

**Arguments:**

- `buffer`: pointer to the memory region from which the image file will be loaded.
  If `size` is one of the constants indicating a [special loading mode][loading-modes], then this argument is the
  corresponding special value.
- `size`: size of the memory buffer pointed to by `buffer`.
  This argument can also be one of the [special loading mode constants][mode-constants] indicating that the image data
  will instead be loaded from a file, from a [`plum_buffer`][buffer] struct, or through callbacks.
  (Note: these special constants take up the highest possible `size_t` values, and therefore the risk of colliding
  with true buffer sizes is minimal; it is possible to use the `PLUM_BUFFER` special mode to mitigate this risk
  completely.)
- `flags`: [color format][colors] and options used to load the image.
  This value is a bitwise OR (`|`) of any number of [loading flags constants][loading-flags], as follows:
    - [Color format][colors] constants, which will determine the color format that the image data will use.
      If this is omitted, the default format (`PLUM_COLOR_32`) will be used; it is _strongly_ recommended to never
      omit this value.
    - `PLUM_ALPHA_REMOVE`: indicates that the image must be loaded as fully opaque, even if it has transparency data;
      the alpha channel will be set to zero (or to its maximal value if `PLUM_ALPHA_INVERT` is specified as part of
      the [color format][colors]) when the image is loaded.
    - One of the [indexed-color mode][indexed] flags:
        - `PLUM_PALETTE_NONE` (default): specifies that the image will use direct-color mode, i.e., disables all
          palette functionality.
          If the image does have a palette, this palette will be removed on load and the pixel data will contain the
          colors directly.
          The `PLUM_PALETTE_NONE` constant has a value of zero, meaning that this mode is the default and will be used
          if none of the modes is specified.
        - `PLUM_PALETTE_LOAD`: instructs the library to load palettes when they exist; the image will use whatever
          mode is specified by the file's data.
          In other words, this mode preserves the image accurately: if it contains a palette, it will load that
          palette; if it doesn't contain one, it will load the pixel colors directly.
        - `PLUM_PALETTE_GENERATE`: indicates that the user wants the image to use indexed-color mode if possible.
          If the image has a palette, that palette will be loaded; in that case, the library will adjust the image's
          `max_palette_index` member if not all colors of the palette are used.
          (This is because this mode already specifies that the library may create palette data.)
          If the image doesn't have a palette, the library will attempt to create one; if the image uses 256 or fewer
          colors, this will succeed, and the image's pixel data will be replaced by color indexes.
          If the image uses more than 256 color values, which is the maximum allowed for a palette, no palette will be
          created and the image's pixel data will contain color values directly.
        - `PLUM_PALETTE_FORCE`: identical to `PLUM_PALETTE_GENERATE`, but it indicates that the user only accepts
          indexed-color mode images.
          If the library cannot create a palette because the image has too many colors, the image will fail to load
          with a [`PLUM_ERR_TOO_MANY_COLORS`][errors] error.
    - One of the palette-sorting constants (which will only be used if a new palette is generated or if sorting an
      existing palette is requested):
        - `PLUM_SORT_LIGHT_FIRST` (default): specifies that the brighest colors should be placed first in the palette.
          This constant has a value of zero, so this ordering will be used by default if none if specified.
        - `PLUM_SORT_DARK_FIRST`: specifies that the darkest colors should be placed first in the palette.
    - `PLUM_SORT_EXISTING`: indicates that, if the image already has a palette (and that palette is being loaded), the
      existing palette should be sorted (and the pixels' index values adjusted accordingly).
      By default, existing palettes are left untouched; only generated palettes are sorted.
- `error`: pointer to an `unsigned` value that will be set to [an error constant][errors] if the function fails.
  If the function succeeds, that value will be set to zero.
  This argument can be `NULL` if the caller isn't interested in the reason why loading failed, as the failure itself
  can be detected through the return value.

**Return value:**

Pointer to the (newly-created) loaded image, or `NULL` on failure.
If the function fails and `error` is not `NULL`, `*error` will indicate the reason.

**Error values:**

If the `error` argument isn't `NULL`, the value it points to will be set to [an error constant][errors].
The error constants signal the following reasons for failure:

- `PLUM_OK` (zero): success.
  This value will be used only if the function succeeds, i.e., it returns a non-`NULL` value.
- `PLUM_ERR_INVALID_ARGUMENTS`: `buffer` is `NULL`.
- `PLUM_ERR_INVALID_FILE_FORMAT`: the image's file format wasn't recognized, or the image was damaged or couldn't be
  loaded for some reason.
  This indicates an error in the image data, such as an unexpected value.
- `PLUM_ERR_INVALID_COLOR_INDEX`: the image file uses [indexed-color mode][indexed] internally, but some of its pixels
  specify a color index that is out of range for the palette it defines.
- `PLUM_ERR_TOO_MANY_COLORS`: the [`PLUM_PALETTE_FORCE`][loading-flags] flag was specified, but a palette couldn't be
  generated because the image has more than 256 distinct color values.
- `PLUM_ERR_UNDEFINED_PALETTE`: the image file uses [indexed-color mode][indexed] internally, but doesn't define its
  palette.
  (This library doesn't support loading images with implicit palettes.)
- `PLUM_ERR_IMAGE_TOO_LARGE`: one of the image's dimensions (`width`, `height` or `frames`) doesn't fit in a 32-bit
  unsigned integer.
- `PLUM_ERR_NO_DATA`: one of the image's dimensions (`width`, `height` or `frames`) is zero.
  (This is possible for some image formats that allow image files to only contain ancillary data for further files to
  use, such as palettes or decoder tables; this library doesn't use data from other files when decoding an image file,
  and therefore those ancillary files cannot be loaded.)
- `PLUM_ERR_FILE_INACCESSIBLE`: the specified file could not be opened.
  This error can only occur when `size` is set to [`PLUM_FILENAME`][mode-constants].
- `PLUM_ERR_FILE_ERROR`: an error occurred while reading the image data.
  This error can only occur when `size` is set to [`PLUM_FILENAME`][mode-constants] or
  [`PLUM_CALLBACK`][mode-constants].
- `PLUM_ERR_OUT_OF_MEMORY`: there is not enough memory available to load the image.

### `plum_store_image`

``` c
size_t plum_store_image(const struct plum_image * image, void * restrict buffer,
                        size_t size, unsigned * restrict error);
```

**Description:**

This function, together with [`plum_load_image`](#plum_load_image), implements most of the library's functionality.
This function will write out an image's data, in the format indicated by its `type` member; this is the only use of
that member, and therefore, the way the user can choose the format in which image data is generated.

The image data can be written out to a memory buffer, a file, an automatically-allocated buffer in a
[`plum_buffer`][buffer] struct, or through a user-defined callback; see the [Loading and storing modes][loading-modes]
page for more information.

If the `size` argument is set to [`PLUM_BUFFER`][mode-constants], the `buffer` argument will point to a
[`plum_buffer`][buffer] struct whose `data` member will be automatically allocated by the function if it succeeds,
using the `malloc` standard function.
In this case, the user is responsible for calling `free` on it after using it; this memory is **not** managed by the
library.
(If the `free` function isn't available, a special mode of [`plum_free`](#plum_free) may be used; see that function's
description for more information.)

The image written out will attempt to mirror the image data to the best of the format's abilities.
For example, if the chosen format supports [indexed-color mode][indexed], and the image uses it, the generated image
data will use that mode as well.
However, not all formats support all of the library's features.
For more information on which formats support which features, see the [Supported file formats][formats] page.

In some cases, the chosen format won't be able to represent the image at all.
(For example, the image may have more than one frame, and the chosen format only supports single-frame images.)
In that case, this function will fail; the error code returned through the `error` parameter will explain the reason
for that failure.

**Arguments:**

- `image`: image that will be written out.
- `buffer`: pointer to the memory region where the image data will be written.
  If `size` is one of the constants indicating a [special storing mode][loading-modes], then this argument is the
  corresponding special value.
- `size`: size of the memory buffer pointed to by `buffer`.
  This argument can also be one of the [special storing mode constants][mode-constants] indicating that the image will
  instead be written out to a file, an automatically-allocated [`plum_buffer`][buffer] struct, or through callbacks.
  (Note: these special constants take up the highest possible `size_t` values, and therefore the risk of colliding
  with true buffer sizes is minimal.)
- `error`: pointer to an `unsigned` value that will be set to [an error constant][errors] if the function fails.
  If the function succeeds, that value will be set to zero.
  This argument can be `NULL` if the caller isn't interested in the reason why writing out the image failed, as the
  failure itself can be detected through the return value.

**Return value:**

If the function succeeds, it will return the number of bytes written.
(If `size` equals [`PLUM_BUFFER`][mode-constants], then the return value will equal the size of the allocated buffer.)
If the function fails, it will return zero (which is never a valid return value on success).
In this case, if `error` is not `NULL`, `*error` will indicate the reason.

**Error values:**

If the `error` argument isn't `NULL`, the value it points to will be set to [an error constant][errors].

This function can fail for any of the reasons specified in the [`plum_validate_image`](#plum_validate_image) function,
setting the error code to that function's return value.
In addition to those results, the following error codes may be set:

- `PLUM_OK` (zero): success.
  This value will be used only if the function succeeds, i.e., it returns a non-`NULL` value.
- `PLUM_ERR_INVALID_ARGUMENTS`: `image` or `buffer` are `NULL`, or `size` is zero.
- `PLUM_ERR_INVALID_FILE_FORMAT`: the image's `type` member is [`PLUM_IMAGE_NONE`][types].
- `PLUM_ERR_INVALID_COLOR_INDEX`: the image uses [indexed-color mode][indexed], but some of the pixels in the image
  have an invalid color index (i.e., a value greater than the image's `max_palette_index`).
- `PLUM_ERR_TOO_MANY_COLORS`: the chosen image format (given by the image's `type` member) only supports images with
  a certain number of colors, and the image uses more than that.
- `PLUM_ERR_IMAGE_TOO_LARGE`: the image's dimensions are larger than what the chosen image format (given by the
  image's `type` member) supports.
  (Note: this does **not** apply when the chosen format only supports single-frame images and the image's `frames`
  member is larger than one; the `PLUM_ERR_NO_MULTI_FRAME` error code is used instead in that case.)
- `PLUM_ERR_NO_MULTI_FRAME`: the chosen image format (given by the image's `type` member) only supports single-frame
  images, but the image has more than one frame (i.e., the image's `frames` member is greater than 1).
- `PLUM_ERR_FILE_INACCESSIBLE`: the specified file could not be opened for writing.
  This error can only occur when `size` is set to [`PLUM_FILENAME`][mode-constants].
- `PLUM_ERR_FILE_ERROR`: an error occurred while writing out the image data.
  This error can only occur when `size` is set to [`PLUM_FILENAME`][mode-constants] or
  [`PLUM_CALLBACK`][mode-constants].
- `PLUM_ERR_OUT_OF_MEMORY`: there is not enough memory available to complete the operation.

* * *

Prev: [Metadata](metadata.md)

Next: Constants and enumerations

Up: [README](README.md)

[buffer]: structs.md#plum_buffer
[colors]: colors.md
[conventions]: conventions.md#Conventions
[errors]: #
[formats]: #
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[loading-flags]: #
[loading-modes]: #
[memory]: memory.md
[metadata]: metadata.md
[mode-constants]: #
[types]: #
