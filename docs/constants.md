# Constants and enumerations

This page lists all constants defined by the library, including constants that are part of `enum` types.

Some `enum` types contain an extra constant at the end indicating the number of constants they define, prefixed with
`PLUM_NUM`.
(For example, the [`PLUM_NUM_IMAGE_TYPES`](#number-of-constants) constant corresponds to the number of image types
defined by the [`enum plum_image_types`](#image-types) type, and is one greater than the highest image type constant
in that `enum`.)
Those constants will be listed separately, in the [Number of constants](#number-of-constants) section.

- [Special loading and storing modes](#special-loading-and-storing-modes)
- Image types
- Metadata node types
- Frame disposal methods
- Errors
- Number of constants

## Special loading and storing modes

These constants are used by the [`plum_load_image`][load] and [`plum_store_image`][store] functions to indicate that
they should use a special [loading or storing mode][loading-modes] instead of reading from or writing to a memory
buffer.

These constants are of `size_t` type, and they don't belong to an `enum`.
Their values are the highest possible values of that type, and therefore they are extremely unlikely to collide with
actual buffer sizes.

- `PLUM_FILENAME`: indicates that the `buffer` argument is a `char *` value containing a filename.
- `PLUM_BUFFER`: indicates that the `buffer` argument is a [`struct plum_buffer *`][buffer] value that describes (or
  will describe) a memory buffer and its size.
- `PLUM_CALLBACK`: indicates that the `buffer` argument is a [`struct plum_callback *`][callback] value, containing a
  callback function that will be called to read or write data.

For more information, see the [Loading and storing modes][loading-modes] page.

* * *

Prev: [Function reference](functions.md)

Next: Macros

Up: [README](README.md)

[buffer]: structs.md#plum_buffer
[callback]: structs.md#plum_callback
[load]: functions.md#plum_load_image
[loading-modes]: #
[store]: functions.md#plum_store_image
