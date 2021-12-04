# Data structures

The main data structure used by the library is the `plum_image` data structure, which represents an image.
All image data is contained by this structure, as well as some minimal associated metadata and linked memory
allocations.
Other structures are used to hold some ancillary data and/or to pass data to functions, and they are also described
in this page.

Note: the library contains no public `typedef` declarations. All named structures are defined as struct tags (e.g.,
`struct plum_image`, not `plum_image`).
C users who prefer `typedef` declarations might want to declare `typedef struct plum_image plum_image_t` instead;
these `_t` suffixed aliases will never be defined by the library and are available for users for this purpose.

- [`plum_image`](#plum_image)
- [`plum_metadata`](#plum_metadata)
- [`plum_buffer`](#plum_buffer)
- [`plum_callback`](#plum_callback)

## `plum_image`

``` c
struct plum_image {
  uint16_t type;
  uint8_t max_palette_index;
  uint8_t color_format;
  uint32_t frames;
  uint32_t height;
  uint32_t width;
  void * allocator;
  struct plum_metadata * metadata;
  union {
    void * palette;
    uint16_t * palette16;
    uint32_t * palette32;
    uint64_t * palette64;
  };
  union {
    void * data;
    uint8_t * data8;
    uint16_t * data16;
    uint32_t * data32;
    uint64_t * data64;
  };
  void * user;
};
```

(Note: in C89 mode, the `palette` and `data` members replace their respective anonymous unions, and the convenience
typed aliases are not available.)

This structure contains an image. An image is fully defined by its size and its pixel data, in the format determined
by its color format, and referencing its palette if any; the remaining members contain some minimal ancillary data
used to aid image processing (and in the case of animations, to describe the animation parameters, which are contained
in the metadata).

- `type`: determines the file type of the image, i.e., the image format.
  This value is set when loading an image (through the [`plum_load_image`][load] function) and copied by the
  [`plum_copy_image`][copy] function; [`plum_new_image`][new] will initialize it to zero (i.e., `PLUM_IMAGE_NONE`).
  It should be set to one of [the image type constants][types].
  This value is only used by the [`plum_store_image`][store] function to determine what file format it will generate.
  An image can be "converted" to a different format by simply changing this value; that will cause the
  [`plum_store_image`][store] function to generate a file in the new format when called.
- `max_palette_index`: maximum valid palette index for [indexed-color images][indexed].
  This limit is inclusive: for example, a value of 2 means that the image uses color indexes between 0 and 2 (and
  therefore that the palette has 3 entries).
  This value is ignored for images that don't use a palette.
- `color_format`: specifies the color format used by the image.
  See the [Color formats][colors] page for more information about the different color formats available.
- `frames`: number of frames in the image; cannot be zero for a valid image.
  Most images will contain a single frame, and therefore this member will be 1; many image formats only support
  single-frame images and will error out if [`plum_store_image`][store] attempts to store an image with `frames > 1`
  in one of those formats.
  All frames in an image are the same size.
- `height`: height of the image, in pixels; cannot be zero for a valid image.
- `width`: width of the image, in pixels; cannot be zero for a valid image.
- `allocator`: internal use member used to track memory allocations linked to an image; must not be modified by the
  application.
  See the [Memory management][memory] page for more information.
  For statically-initialized images, this member must be initialized to a null pointer.
  Images created through one of the [constructor functions][constructors] will initialize this member accordingly.
- `metadata`: linked list of [`struct plum_metadata`](#plum_metadata) nodes containing image metadata.
  Can be `NULL` if the image contains no metadata.
  See the [Metadata][metadata] page for more information.
- `palette`: palette data for the image.
  If the image uses [indexed-color mode][indexed], this member will point to an array of colors that define the
  image's palette; see the [Color formats][colors] page for more information on how the colors are stored.
  If the image doesn't use a palette, this member will be `NULL`; that is what determines whether an image uses
  [indexed-color mode][indexed] or not.
- `data`: pixel data for the image; it is a frame-major, then row-major array of pixel values.
  (For example, for an image with `frames = 3`, `height = 20`, `width = 10`, index 1 of this array will access the
  pixel at X = 1, Y = 0 in the first frame; index 10 will access the pixel at X = 0, Y = 1; and index 200 will access
  the top left corner of the second frame.)
  For more information about how this data is stored, see [the section on accessing pixel data][accessing].
- `user`: free pointer member intended for the user to store any data they want to associate to the image.
  This member is not used in any way by the library.
  The [`plum_new_image`][new] and [`plum_load_image`][load] functions initialize this member to `NULL`.
  The [`plum_copy_image`][copy] function will copy the value from the image it's copying; note that this will
  necessarily be a shallow copy (i.e., only the pointer is copied), since the library doesn't know the structure of
  the user data.

In C++ mode, the `struct plum_image` type will also contain a number of helper methods that allow easy access to the
pixel and palette data. See the [C++ helper methods][helpers] page for more information.

## `plum_metadata`

``` c
struct plum_metadata {
  int type;
  size_t size;
  void * data;
  struct plum_metadata * next;
};
```

This structure represents a single node of metadata; metadata is stored as a linked list in the image structure.
For more information, see the [Metadata][metadata] page.

- `type`: metadata type for the current node.
  It can be a positive [metadata type constant][metadata-constants], zero (`PLUM_METADATA_NONE`) to signal that the
  node is unused, or a negative application-defined type.
- `size`: size of the data in the node.
- `data`: pointer to data in the node.
  If `size` is zero, this member may be a null pointer.
  Each metadata type defines the format for the data in its nodes; see the [Metadata][metadata] page for more
  information.
- `next`: pointer to the next metadata node in the image.
  If the current node is the last node, this member is a null pointer; this behaves like an ordinary linked list.

## `plum_buffer`

``` c
struct plum_buffer {
  size_t size;
  void * data;
};
```

This structure is used as the source for [`plum_load_image`][load] or the destination for [`plum_store_image`][store]
when the `size` argument (which is also used to signal special sources/destinations) is set to
[the `PLUM_BUFFER` constant][size-constants].

The structure describes a data buffer in a self-explanatory way.

## `plum_callback`

``` c
struct plum_callback {
  int (* callback) (void * userdata, void * buffer, int size);
  void * userdata;
};
```

This structure is used as the source for [`plum_load_image`][load] or the destination for [`plum_store_image`][store]
when the `size` argument is set to [the `PLUM_CALLBACK` constant][size-constants], indicating that data should be read
from or written to a user-defined callback function.
This allows implementing data readers and writers for any user-defined resource without having to add special code for
that resource type in the library, thus enabling those functions to interact with image data from anywhere.

For more information, see the [Data callbacks][callbacks] page.

- `callback`: the function that will be called for each data read/write.
  The `userdata` argument will be the value from the `plum_callback` struct, the `buffer` argument will be memory
  buffer where the callback should read from or write to, and the `size` argument is the number of bytes to read or
  write.
  The `size` argument will never be larger than `0x7fff`.
  The function must return the number of bytes read or written (which may be less than the number requested, but not
  more), zero for end of file (only when reading) or a negative value for an error.
  A return value of zero for a write callback will not be considered an end-of-file condition and will simply retry
  the write.
- `userdata`: value that will be passed to the callback as its first argument.

* * *

Prev: [Color formats](colors.md)

Next: [Memory management](memory.md)

Up: [README](README.md)

[accessing]: colors.md#accessing-pixel-and-color-data
[callbacks]: #
[colors]: colors.md
[constructors]: #
[copy]: #
[helpers]: #
[indexed]: colors.md#indexed-color-mode
[load]: #
[memory]: memory.md
[metadata]: metadata.md
[metadata-constants]: #
[new]: #
[size-constants]: #
[store]: #
[types]: #
