# Changelog

Note: releases are listed from latest to oldest.

## Version 1.2 (in development)

- Added `PLUM_METADATA_FRAME_AREA` metadata node when reading GIF, APNG and PNM files, preserving the sizes of the
  frames read in
- Updated image generators for GIF, APNG and PNM formats, to handle `PLUM_METADATA_FRAME_AREA` metadata when possible
- Fixed alignment errors when `alignof(jmp_buf) > alignof(max_align_t)`
- Fixed undefined behavior when reading PNG files due to array index overflow after signed to unsigned conversion
- Fixed memory leak on `plum_realloc` failure
- Corrected a mistake in the documentation for `plum_store_image`
- Improved the performance of sorting operations (such as color sorting when generating a palette)

## Version 1.1 (9 May 2022)

- Fixed palette reduction for palettes with more than 128 colors
- Fixed invalid memory accesses in PNG file generation that could occur if `uint_fast8_t` was as wide as (or wider
  than) `ptrdiff_t`
- Fixed and improved the shell script that generates the `libplum.c` and `libplum.h` files, making it more resilient
  to unusual environments (such as paths with spaces in them)
- Allowed loading PNG and APNG files with invalid background or transparent colors (which are now ignored)
- Allowed loading JPEG files containing extraneous data after the end marker
- Added support for JPEG files without explicit Huffman tables (using the recommended tables from the standard),
  following the behavior of many common JPEG viewers
- Prevented bitmasked BMP files with completely empty bitmasks from loading (erroring out with `PLUM_ERR_NO_DATA`)
- Adjusted frame durations when generating animations to accumulate rounding errors into subsequent frames, preserving
  the average frame rate of longer animations

## Version 1.0 (14 February 2022)

- Fixed `plum_sort_palette` ensuring that no colors are exchanged in the image
- Fixed handling of simultaneous local and global palettes in GIF images where the local palette was a superset of the
  global palette
- Fixed some rare cases of JPEG component detection and handling
- Added new function `plum_sort_palette_custom` (with its corresponding documentation)
- Renamed the `user` member of the `plum_image` struct to `userdata`
- Added and documented new `PLUM_MAX_MEMORY_SIZE` constant
- Renamed `size` argument of `plum_load_image` and `plum_store_image` to `size_mode`
- Renamed `PLUM_FILENAME`, `PLUM_BUFFER` and `PLUM_CALLBACK` constants to `PLUM_MODE_FILENAME`, `PLUM_MODE_BUFFER` and
  `PLUM_MODE_CALLBACK` (the older constants are currently still available, but undocumented, for compatibility)
- Added new documentation page on handling untrusted images

## Version 0.4 (1 February 2022)

- Fixed a bug when loading APNG files with reduced frames
- Fixed emitting GIF files with transparent pixels
- Fixed a bug in the GIF compressor that would generate invalid compressed frame data when a code size increase and
  reduction occured at the same time
- Fixed a file descriptor leak that would keep an open `FILE *` if a `PLUM_ERR_FILE_ERROR` error was raised while
  reading from a file
- Fixed a bug that prevented non-vertically-flipped BMP files from ever loading
- Fixed read-past-the-end bug when loading monochrome BMP files with a width that is a multiple of 8
- Added safeguards to prevent internal `longjmp` misuse when generating image files
- Ensured that the `PLUM_FILENAME`, `PLUM_BUFFER` and `PLUM_CALLBACK` constants are always `size_t` as documented
- Enforced the size limitation on the value returned by a callback when using the `PLUM_CALLBACK` loading/storing mode
- Added a validation for `PLUM_BUFFER` arguments to `plum_load_image`, ensuring that the buffer's `data` member isn't
  a null pointer
- Handled out-of-palette background colors when generating a GIF file, ensuring that they would never cause the
  process to fail (the background is ignored instead if there are no available palette slots)
- Improved the GIF loader to account for looping extensions that don't appear at the beginning of the file (as long as
  there is at most one per file)
- Added some warning flags for debug builds, and cleared some warnings that would be raised by them
- Added and improved some safety checks that detect maliciously-crafted and other pathological files
- Added detection for empty BMP and GIF files (erroring out with `PLUM_ERR_NO_DATA` instead of
  `PLUM_ERR_INVALID_FILE_FORMAT`)
- Allowed decompressing BMP files that end their last line with a row end marker (and maybe no data end marker)
- Improved PNG compression by fixing a lookback bug in the compressor and adjusting the lookback length
- Prevented BMP images with a height of `0x80000000` from loading, since this is a negative 32-bit value that has no
  positive counterpart (erroring out with `PLUM_ERR_INVALID_FILE_FORMAT`)
- Ensured that GIF frames are always read with a non-zero duration (frames with an instant duration will contain a
  duration of 1 nanosecond in the `PLUM_METADATA_FRAME_DURATION` metadata node, as documented)
- Added and documented a restriction requiring `size_t` to be at least 32 bits wide
- Updated documentation for `plum_load_image` to indicate that a `PLUM_ERR_IMAGE_TOO_LARGE` error may also occur if
  the image's overall dimensions are too large
- Some minor documentation updates and code cleanup

## Version 0.3 (10 January 2022)

- New functions: `plum_load_image_limited`, `plum_check_limited_image_size`, `plum_append_metadata`
- Added a missing check for an extremely unlikely memory allocation failure
- Fixed some BMP and JPEG encoding bugs that could arise under unusual circumstances
- Fixed BMP decoder to accept images that are exactly `0x7fffffff` pixels tall or wide
- Fixed palette generation for images with many similar colors
- Updated the tutorial to use `plum_append_metadata` where relevant
- Some refactorings and minor documentation changes

## Version 0.2 (4 January 2022)

- Fixed a number of decoding bugs involving unusual image parameters
- Fixed a bug with `plum_malloc` and related functions underflowing the allocation size on excessively large arguments
- Updated the documentation to show that `data` and `palette` aliases in the `plum_image` struct exist in C11, not C99

## Version 0.1 (2 January 2022)

First release.

* * *

Prev: [Library versioning](version.md)

Up: [README](README.md)
