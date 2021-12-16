# C++ helper methods

The [`plum_image` struct][image] contains image data in a series of arrays.
However, these arrays are multidimensional, which makes accessing them awkward, as they are referenced through a
single pointer.
These helper methods provide a way of accessing those arrays from C++ code, as a counterpart to the
[VLA-based macros][vla] that are available in C99 and later.

- [Introduction](#introduction)
- [Pixel accessors](#pixel-accessors)
- [Palette accessors](#palette-accessors)

## Introduction

While it is possible to access an image's arrays directly, it is often impractical to do so.
For example, pixel arrays are linear, which requires a coordinate computation in order to locate a specific pixel in
those arrays.

However, the [VLA-based macros][vla] that the library header makes available don't solve the problem in C++, since
C++ has no support for VLAs.
Therefore, another solution is needed for C++ mode.

Accessor methods provide this solution, returning a reference to a specific color or pixel value within those arrays.
Accessor methods are part of the [`plum_image` struct][image] when in C++ mode, and they look like this:

``` cpp
inline uint32_t & pixel32 (uint32_t col, uint32_t row, uint32_t frame = 0) {
  return ((uint32_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline const uint32_t & pixel32 (uint32_t col, uint32_t row, uint32_t frame = 0) const {
  return ((const uint32_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}
```

These methods always return an lvalue reference, and they exist in `const` and non-`const` versions.

Note: in the lists below, only the non-`const` versions are shown, for brevity.
The `const` versions of the methods parallel the non-`const` versions except for the two uses of the keyword, as shown
in the example above.

## Pixel accessors

- `inline uint8_t & pixel8 (uint32_t col, uint32_t row, uint32_t frame = 0);`
- `inline uint16_t & pixel16 (uint32_t col, uint32_t row, uint32_t frame = 0);`
- `inline uint32_t & pixel32 (uint32_t col, uint32_t row, uint32_t frame = 0);`
- `inline uint64_t & pixel64 (uint32_t col, uint32_t row, uint32_t frame = 0);`

These methods access the image's `data` member as an array of 8-bit, 16-bit, 32-bit or 64-bit pixel values.
The method that must be used depends on the image's [color format][colors] and on whether it uses
[indexed-color mode][indexed] or not.
(For more information, see the [Accessing pixel and color data][accessing] section.)

The arguments to these methods are the coordinates of the pixel being accessed: the column (X coordinate), the row (Y
coordinate) and the frame number.
(The frame number may be left out if the first frame is being accessed.)

Note that these methods will _not_ perform any bounds checking when accessing the requested pixel.

## Palette accessors

- `inline uint16_t & color16 (uint8_t index);`
- `inline uint32_t & color32 (uint8_t index);`
- `inline uint64_t & color64 (uint8_t index);`

These methods access the image's `palette` member as an array of 16-bit, 32-bit or 64-bit color values.
They are functionally equivalent to `image -> palette32[index]` and the like; they are provided for completeness, for
users that prefer accessor methods to direct array access using an anonymous union.

These methods, like the [pixel accessor](#pixel-accessors) methods, will _not_ perform any bounds checking when
accessing the palette.

* * *

Prev: [Loading and storing modes](modes.md)

Next: [Supported file formats](formats.md)

Up: [README](README.md)

[accessing]: colors.md#accessing-pixel-and-color-data
[colors]: colors.md
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[vla]: macros.md#pixel-array-macros
