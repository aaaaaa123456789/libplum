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
    - [`plum_destroy_image`](#plum_destroy_image)
- [Validation](#validation)
    - [`plum_validate_image`](#plum_validate_image)
    - [`plum_check_valid_image_size`](#plum_check_valid_image_size)
    - [`plum_validate_palette_indexes`](#plum_validate_palette_indexes)
- [Color operations](#color-operations)
    - [`plum_convert_color`](#plum_convert_color)
    - [`plum_convert_colors`](#plum_convert_colors)
    - [`plum_remove_alpha`](#plum_remove_alpha)
    - [`plum_sort_colors`](#plum_sort_colors)
    - [`plum_color_buffer_size`](#plum_color_buffer_size)
- [Palette operations](#palette-operations)
    - [`plum_convert_colors_to_indexes`](#plum_convert_colors_to_indexes)
    - [`plum_convert_indexes_to_colors`](#plum_convert_indexes_to_colors)
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

Pointer to the (newly-created) loaded [`struct plum_image`][image], or `NULL` on failure.
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
that member after an image has been loaded, and therefore, the way the user can choose the format in which image data
is generated.

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

### `plum_destroy_image`

``` c
void plum_destroy_image(struct plum_image * image);
```

**Description:**

This function deallocates all memory resources associated to an image.

If the image has been created with one of the [constructor functions](#basic-functionality), then this function will
destroy the image itself, along with all buffers created by the function that created it.
Regardless of how the image was created, this function will also deallocate any memory associated to the image that
has been allocated by [`plum_malloc`](#plum_malloc), [`plum_calloc`](#plum_calloc), [`plum_realloc`](#plum_realloc) or
[`plum_allocate_metadata`](#plum_allocate_metadata) and hasn't been released yet by [`plum_free`](#plum_free).

If this function is called on an image created with one of the [constructor functions](#basic-functionality), since
the image itself will be destroyed, that struct cannot be used at all after the call.
(It must be considered freed.)
On the other hand, if the image was statically allocated (or allocated by the program without invoking one of the
library's functions), this function will reset the image's `allocator` member, which will allow reusing it.
However, in that case, if any of the buffers referenced by the image's members has been allocated by the
[memory management functions](#memory-management), those buffers will have been freed, so special care must be taken.

If the image was not created by one of the [constructor functions](#basic-functionality) and no memory is associated
to it, this function will do nothing.
It will also do nothing if the `image` argument is `NULL`.
Therefore, it is always safe (and recommended) to call this function when the user will stop using an image.

**Arguments:**

- `image`: pointer to the image that will be destroyed.

**Return value:** none.

## Validation

These functions validate various aspects of an image, allowing the user to detect errors before attempting to write it
out to a file.

### `plum_validate_image`

``` c
unsigned plum_validate_image(const struct plum_image * image);
```

**Description:**

This function will validate the contents of a [`plum_image`][image] struct, ensuring that all of its members are set
to reasonable values.
It will also validate the contents of known [metadata nodes][metadata], checking that their data sizes match what is
expected and that their contents are valid.
(Metadata nodes with negative types won't be checked, since their structure is defined by the user, not the library.)

Note: this function will _not_ check pixel values for validity, since that check alone would be significantly slower
than all other checks combined.
Use the [`plum_validate_palette_indexes`](#plum_validate_palette_indexes) function for that purpose.
(That function can also quickly handle images that don't need any validation, such as images that don't use
[indexed-color mode][indexed] in the first place.)

**Arguments:**

- `image`: pointer to the image that will be validated.

**Return value:**

If validation is successful, the function returns zero ([`PLUM_OK`][errors]).
If validation fails, the function will return a non-zero [error constant][errors] indicating the check that failed.

**Error values:**

These are the possible error values that the function can return:

- `PLUM_OK` (zero): success.
  This value will be returned only if all checks pass, i.e., if the image is valid.
- `PLUM_ERR_INVALID_ARGUMENTS`: `image` is `NULL`.
- `PLUM_ERR_INVALID_FILE_FORMAT`: the image's `type` member is set to an invalid value.
  (Note that the [`PLUM_IMAGE_NONE`][types] type will _not_ be considered invalid by this function.
  However, [`plum_image_store`](#plum_image_store) will still refuse to write out an image with that type.)
- `PLUM_ERR_INVALID_METADATA`: one of the image's [metadata nodes][metadata] has an unknown type (i.e., a positive
  type that is not one of the types defined by the library), a non-zero `size` with a null `data` pointer, or a size
  or contents that don't fulfill the constraints given in the [Metadata][metadata] page.
- `PLUM_ERR_IMAGE_TOO_LARGE`: the image's dimensions are too large for the pixel buffer size to fit in a `size_t`
  value.
  This is checked with the [`plum_check_valid_image_size`](#plum_check_valid_image_size) function.
- `PLUM_ERR_NO_DATA`: either the image's `data` member (i.e., its pixel data) is a null pointer, or one of the image's
  dimensions is zero, indicating that the image has no pixels.

### `plum_check_valid_image_size`

``` c
int plum_check_valid_image_size(uint32_t width, uint32_t height,
                                uint32_t frames);
```

**Description:**

This function verifies that the size of an image's pixel buffer can be represented as a `size_t` value, regardless of
the image's [color format][colors] and whether it uses [indexed-color mode][indexed] or not.

In other words, it checks that `width * height * frames * sizeof(uint64_t)` doesn't overflow a `size_t`.

Note that it does _not_ check whether that size will fit in memory, or if it is at all reasonable.
This function only checks for overflow; it is intended to prevent security issues that derive from overflowing sizes.

**Arguments:**

- `width`: width of the image.
- `height`: height of the image.
- `frames`: number of frames in the image.

(Note that, since the function's purpose is to validate a product, the arguments are actually interchangeable.)

**Return value:**

1 if the size is valid, or 0 if it overflows.

### `plum_validate_palette_indexes`

``` c
const uint8_t * plum_validate_palette_indexes(const struct plum_image * image);
```

**Description:**

This function validates the pixel values of an image.

If an image uses [indexed-color mode][indexed], pixel values (i.e., indexes) must be smaller than or equal to the
image's `max_palette_index` value.
This function checks those values and finds the first value that is out of range.

Images that use full 256-color palettes, as well as images that don't use [indexed-color mode][indexed] at all, don't
need any validation, since all pixel values are valid for those images.
This function handles those images quickly and returns `NULL` for them, ensuring that it can be called for any image.

**Arguments:**

- `image`: image to validate.

**Return value:**

`NULL` if all pixel values are valid (or if `image` itself is `NULL`).
Pointer to the first invalid pixel (i.e., a pointer within `image -> data8`) otherwise.

## Color operations

These functions perform various operations and transformations on color values directly, such as converting between
different [color formats][colors].

### `plum_convert_color`

``` c
uint64_t plum_convert_color(uint64_t color, unsigned from, unsigned to);
```

**Description:**

This function converts a single color value from any [color format][colors] to any other.

The color formats are specified using the constants described in the [Color formats][colors] page; only the color
format bits are used by this function (in other words, other flags are ignored).

**Arguments:**

- `color`: color value to convert.
- `from`: `color`'s current [color format][colors].
- `to`: [color format][colors] to convert to.

**Return value:**

Converted color value.
The result is unspecified if `color` is out of range for the format specified by `from`.

### `plum_convert_colors`

``` c
void plum_convert_colors(void * restrict destination,
                         const void * restrict source, size_t count,
                         unsigned to, unsigned from);
```

**Description:**

This function converts an array of color values from any [color format][colors] to any other.
It is equivalent to calling [`plum_convert_color`](#plum_convert_color) on each value in the array.
However, this function will automatically handle converting between [color formats][colors] of different sizes.

The `destination` buffer must be large enough to store `count` colors in the format specified by `to`; this might not
be the same size as the `source` array if the specified color formats are of different sizes.
As indicated by the `restrict` keyword, the `source` and `destination` arrays must not overlap.

Note that the `to` and `from` arguments are in reverse order with respect to the
[`plum_convert_color`](#plum_convert_color) function.
This was done to match the order of the `destination` and `source` arguments.

The color formats are specified using the constants described in the [Color formats][colors] page; only the color
format bits are used by this function (in other words, other flags are ignored).

**Arguments:**

- `destination`: buffer where the converted color values will be stored; it must be large enough to store `count`
  color values in the format specified by `to`.
  This argument must not be `NULL` unless `count` is zero.
- `source`: buffer containing the color values to be converted; it must contain `count` color values in the format
  specified by `from`, and it must not overlap with `destination`.
  This argument must not be `NULL` unless `count` is zero.
- `count`: number of color values to convert.
  If this value is zero, no conversions are performed.
- `to`: [color format][colors] to convert to.
- `from`: current [color format][colors] for the values in `source`.

**Return value:** none.

### `plum_remove_alpha`

``` c
void plum_remove_alpha(struct plum_image * image);
```

**Description:**

This function removes transparency data from an image, in the same way the [`PLUM_ALPHA_REMOVE`][loading-flags] flag
does when loading an image with [`plum_load_image`](#plum_load_image).

The function operates in place and modifies the image's color or palette values, making them all opaque.
(In other words, it sets the alpha channel of all color values to zero, or to its maximal value if the image's
[color format][colors] includes the [`PLUM_ALPHA_INVERT`][loading-flags] flag.)

**Arguments:**

- `image`: image to transform.
  If this argument is `NULL`, the function does nothing.

**Return value:** none.

### `plum_sort_colors`

``` c
void plum_sort_colors(const void * restrict colors, uint8_t max_index,
                      unsigned flags, uint8_t * restrict result);
```

**Description:**

This function sorts an array of colors, in the same way [`plum_load_image`](#plum_load_image) does when generating a
new palette (or sorting an existing one).

The function writes out an array of indexes containing the result of the sort.
For example, if `result[0]` is 36, that means that index 36 in the original array corresponds to the first color after
sorting.
The function **does not** modify the original array.
This is because the function's primary use is to sort an image's colors, and doing that requires sorting the palette
and updating the index values for all pixels; a sorted array of indexes allows the user to perform both operations.
(To do all of this automatically for an image, use the [`plum_sort_palette`](#plum_sort_palette) function instead.)

**Arguments:**

- `colors`: array of colors to sort.
  Must not be `NULL`.
- `max_index`: maximum valid index into `colors`, in the same manner the `max_palette_index` member of a
  [`struct plum_image`][image] is specified.
  Note that this is _one less_ than the number of colors in the array.
- `flags`: [color format][colors] and sorting mode used to sort the array.
  This value is a bitwise OR (`|`) of the following values, taken from the [loading flags constants][loading-flags]
  (other bits from those flags will be ignored):
    - [Color format][colors] constants, which determine the color format used for the values in the `colors` array.
      If this is omitted, the default format (`PLUM_COLOR_32`) will be used; it is _strongly_ recommended to never
      omit this value.
    - One of the palette-sorting constants:
        - `PLUM_SORT_LIGHT_FIRST` (default): specifies that the brighest colors should be sorted first.
          This constant has a value of zero, so this ordering will be used by default if none if specified.
        - `PLUM_SORT_DARK_FIRST`: specifies that the darkest colors should be sorted first.
- `result`: array of `uint8_t` values where the sorted indexes will be stored.
  Must not overlap with `colors`.

**Return value:** none.

### `plum_color_buffer_size`

``` c
size_t plum_color_buffer_size(size_t size, unsigned flags);
```

**Description:**

This function calculates the size that a buffer would have to have to be able to contain a given number of color
values using a chosen [color format][colors].
This can be used to calculate allocation sizes, `memcpy` sizes, etc.

**Arguments:**

- `size`: number of colors in the buffer.
- `flags`: [color format][colors] that the buffer will use.
  This argument is named `flags` after the argument to [`plum_load_image`](#plum_load_image), but only the color
  format bits are used; other [loading flags][loading-flags] are ignored.

**Return value:**

If `size` is valid (i.e., `size * sizeof(uint64_t)` doesn't overflow a `size_t`), this function returns the number of
bytes that many colors would take.
(In particular, that means that `plum_color_buffer_size(1, format)` can be used to obtain the size of a single color
value for any given [color format][colors].)

If `size` is not valid (i.e., it would overflow for some color formats), the function returns 0.
This happens regardless of whether the specific color format indicated by `flags` would cause the calculation to
overflow.

## Palette operations

These functions operate on palettes, assisting the user in converting images to and from [indexed-color mode][indexed]
and in manipulating the palettes already there.

### `plum_convert_colors_to_indexes`

``` c
int plum_convert_colors_to_indexes(uint8_t * restrict destination,
                                   const void * restrict source,
                                   void * restrict palette,
                                   size_t count, unsigned flags);
```

**Description:**

This function generates a palette out of an array of color values, as well as the corresponding array of indexes.

The library only supports palettes of up to 256 colors; if the array contains more than 256 distinct color values,
generating the palette will fail.

If generating the palette succeeds, the function will convert the color values in `source` to indexes into the
palette, which will be stored in `destination`.
The palette itself will be written to `palette`.
Therefore, this function can be used to convert an image to [indexed-color mode][indexed], generating both the pixel
data (for the [`plum_image`][image] struct's `data` member) and its palette (for the image's `palette` member).

As indicated by the `restrict` keyword, the `destination`, `source` and `palette` buffers must not overlap.

**Arguments:**

- `destination`: buffer where the new palette indexes will be written if the function succeeds.
  Must not be `NULL`.
- `source`: array of color values, in the [color format][colors] given by the `flags` argument.
  Must not be `NULL`.
- `palette`: buffer where the palette colors will be written if the function succeeds; it should have enough space for
  up to 256 color values (in the [color format][colors] specified by the `flags` argument), since that is the maximum
  number of colors that a palette can use.
  Must not be `NULL`.
- `count`: number of color values in the `source` array (and therefore, number of indexes that will be written to
  `destination` on success).
  Must not be zero.
- `flags`: [color format][colors] and sorting mode used for `source` and `palette`.
  This value is a bitwise OR (`|`) of the following values, taken from the [loading flags constants][loading-flags]
  (other bits from those flags will be ignored):
    - [Color format][colors] constants, which determine the color format used for the values in the `source` array,
      as well as the color format that `palette` will use.
      If this is omitted, the default format (`PLUM_COLOR_32`) will be used; it is _strongly_ recommended to never
      omit this value.
    - One of the palette-sorting constants, which will be used for the generated palette:
        - `PLUM_SORT_LIGHT_FIRST` (default): specifies that the brighest colors should be sorted first.
          This constant has a value of zero, so this ordering will be used by default if none if specified.
        - `PLUM_SORT_DARK_FIRST`: specifies that the darkest colors should be sorted first.

**Return value:**

If this function succeeds, it returns the maximum index value used by the palette, in the same manner that the
`max_palette_index` member of a [`struct plum_image`][image] is specified.
Note that this is _one less_ than the number of colors in the palette; in particular, it may be zero if the palette
only contains one color.
(The maximum possible return value is 255, for a 256-color palette.)

If the function fails, it will return a negative [error constant][errors].
(For example, if a `PLUM_ERR_TOO_MANY_COLORS` error occurs, the function will return `-PLUM_ERR_TOO_MANY_COLORS`.)
Since all error constants are positive, the function will always return a negative value on error.

**Error values:**

If the return value is negative, it will be the negated value of one of the following [error constants][errors]:

- `PLUM_ERR_INVALID_ARGUMENTS`: one of the pointer arguments is `NULL`, or `size` is zero.
- `PLUM_ERR_TOO_MANY_COLORS`: the `source` array contains more than 256 distinct color values, and therefore a palette
  cannot be generated.
- `PLUM_ERR_OUT_OF_MEMORY`: there isn't enough memory available to complete the operation.

### `plum_convert_indexes_to_colors`

``` c
void plum_convert_indexes_to_colors(void * restrict destination,
                                    const uint8_t * restrict source,
                                    const void * restrict palette,
                                    size_t count, unsigned flags);
```

**Description:**

This function converts an array of palette indexes, along with its corresponding palette, into an array of color
values.
In other words, it is the inverse of [`plum_convert_colors_to_indexes`](#plum_convert_colors_to_indexes).

Unlike that function, this function's conversion will always succeed, as it is always possible to represent colors as
an array of color values.

As indicated by the `restrict` keyword, the `destination`, `source` and `palette` buffers must not overlap.

**Arguments:**

- `destination`: buffer where the color values will be written; it must have enough space for `count` values in the
  [color format][colors] determined by `flags`.
  Must not be `NULL`.
- `source`: array of `count` palette indexes that will be converted.
  Must not be `NULL`.
- `palette`: color palette used by `source`; it uses the [color format][colors] determined by `flags`, and it must
  have as many entries as needed for all indexes in `source` to be valid.
  (For instance, if the indexes in `source` are all between 0 and 4, `palette` must contain at least 5 entries.)
  Must not be `NULL`.
- `count`: number of indexes in `source`, and therefore number of color values that will be written to `destination`.
- `flags`: [color format][colors] that `palette` and `destination` will use.
  This argument is named `flags` after the argument to [`plum_load_image`](#plum_load_image), but only the color
  format bits are used; other [loading flags][loading-flags] are ignored.

**Return value:** none.

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
