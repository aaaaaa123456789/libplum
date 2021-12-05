# Function reference

This page lists all functions exposed by the library's API, their arguments and return values, and their behavior.
The functions are grouped by functionality, but an alphabetical list (with links) is provided at the end.

As it is mentioned in the [Conventions][conventions] page, all functions that take a pointer to `const` as an argument
treat the `const` qualifier as deep.
In other words, the function won't modify any of the data accessible through that pointer.

- [Basic functionality](#basic-functionality)
    - [`plum_new_image`](#plum_new_image)
    - [`plum_copy_image`](#plum_copy_image)
    - `plum_load_image`
    - `plum_store_image`
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

**Arguments:** none.

**Return value:**

Pointer to the new image, or `NULL` on failure due to memory exhaustion.

**Description:**

This function allocates a new image (i.e., a new [`struct plum_image`][image]), returning a pointer to it.
The image is zero-initialized, except for the `allocator` member, which is initialized appropriately.
This image struct is allocated as if by [`plum_malloc`](#plum_malloc), and will be deallocated by
[`plum_destroy_image`](#plum_destroy_image); see the [Memory management][memory] page for more details.

### `plum_copy_image`

``` c
struct plum_image * plum_copy_image(const struct plum_image * image);
```

**Arguments:**

- `image`: image to be copied.

**Return value:**

Pointer to the copied image, or `NULL` on failure.

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

* * *

Prev: [Metadata](metadata.md)

Next: Constants and enumerations

Up: [README](README.md)

[conventions]: conventions.md#Conventions
[image]: structs.md#plum_image
[memory]: memory.md
