# Changelog

Note: releases are listed from latest to oldest.

## Version 1.0 (in development)

- Ensured that the `PLUM_FILENAME`, `PLUM_BUFFER` and `PLUM_CALLBACK` constants are always `size_t` as documented
- Enforced the size limitation on the value returned by a callback when using the `PLUM_CALLBACK` loading/storing mode
- Added some warning flags for debug builds, and cleared some warnings that would be raised by them
- Some minor documentation updates

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
