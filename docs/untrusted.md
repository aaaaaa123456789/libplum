# Handling untrusted input

Most users of the library don't need to consider the security implications of handling an image: images contain no
executable code, and damaged or invalid files are processed correctly by the library.
However, if part of the input processed by the library may be controlled by a malicious actor (for instance, a user of
an Internet-facing service uploading an image), addressing potential vulnerabilities becomes a necessity.
For example, it is possible to exhaust the system's memory by making the library process a very large image, leading
to a denial-of-service attack.

The library therefore contains a number of features that may be used to address potential vulnerabilities and mitigate
the risks associated with handling untrusted input.

- [Untrusted image files](#untrusted-image-files)
- [Untrusted image data](#untrusted-image-data)

## Untrusted image files

The largest risk associated with untrusted image files aren't invalid files (as those will be detected and fail to
load), but valid files with deceptive sizes.
Certain image file formats allow for a very efficient encoding of empty space in the image, which in turn allows for
extremely large images (in terms of pixels) to be stored in very small files.
Those images are extremely unlikely to be useful, since only a tiny portion of them is actually used, but they may be
used by a malicious user to force a program to load an extremely large image.

For example, consider the following hexadecimal dump of a GIF image:

```
00000000  47 49 46 38 39 61 ff ff  ff ff f0 00 00 ff 00 00  |GIF89a..........|
00000010  00 00 00 21 f9 04 05 00  00 01 00 2c ff 7f ff 7f  |...!.......,....|
00000020  01 00 01 00 00 02 02 44  01 00 3b                 |.......D..;|
```

That file is only 43 bytes long, which would appear to represent a small image.
However, that image is actually 65,535 by 65,535 pixels large, with a single red dot in the center (and transparency
elsewhere).

There is nothing invalid about that image: it is perfectly conformant with the GIF specification, and the library can
and will load it.
But if [indexed-color mode][indexed] isn't enabled when loading it, converting that image to direct-color mode will
take up over 32 GB of memory, most likely crashing the program.

The main security feature the library offers to defend against this kind of malicious file is the
[`plum_load_image_limited`][load-limited] function.
This function is equivalent to [`plum_load_image`][load], but it takes an additional `limit` parameter that specifies
the maximum number of pixels an image may have.
Attempting to load a larger image will fail with [`PLUM_ERR_IMAGE_TOO_LARGE`][errors], without attempting to actually
load the pixel buffer; this avoids using excessive amounts of memory (as in the example shown above), as well as
spending a very long time decompressing such an image.

Without using this function, the library will not attempt to limit the size of images it loads, as long as the size of
the pixel buffer would fit in a `size_t` (as checked by the [`plum_check_valid_image_size`][check-size] function); the
library has no hard limits on image sizes, since there are legitimate use cases for very large images and the library
cannot know what sizes are reasonable for a specific use case.
This function allows the user to explicitly tell the library what sizes are reasonable, mitigating the impact of
malicious images that decode to overly-large frames.

**Warning:** this function is intended to mitigate the risk of decoding a very large image compressed into a very
small file.
This is typically only encountered when a malicious user forces a program they don't control to decode such an image
as part of an attack, such as a denial-of-service attack.
Programs that don't need to mitigate this risk (like local console-based or interactive image editors) most likely
won't need this function at all.

## Untrusted image data

Normally, applications won't let users generate a [`struct plum_image`][image] directly.
Therefore, the application can ensure that the data contained in said image is valid.
However, if an application (or a library wrapping this library) exposes an API where users may supply an image, it
becomes possible for an attacker to generate malicious image data that may compromise the application.

It is also possible to compromise an application simply by being able to supply image dimensions to it.
For example, consider a [`struct plum_image`][image] declared with the following fields (amongst other data):

``` c
struct plum_image image = {
  .color_format = PLUM_COLOR_32,
  .width = 2147418113,
  .height = 429509837,
  .frames = 5
};
```

It should be obvious that those image dimensions are nonsensically large.
Attempting to allocate an image buffer for that image should obviously fail.
Notwithstanding, a call to `malloc(sizeof(uint32_t) * image.width * image.height * image.frames)` is very likely to
succeed.
The reason behind that is that, on a 32-bit or 64-bit system, that size calculation amounts to 4 due to overflow.
As a consequence of this overflow, an application that accepts those dimensions and attempts to allocate and use a
buffer for the resulting image will access memory past the end of the allocated buffer, potentially compromising its
data or its integrity.

The library provides several functions to validate image data:

- [`plum_validate_image`][validate] is the primary function intended for validation.
  It checks for all sorts of errors in the [`plum_image`][image] struct, such as invalid dimensions, zero dimensions,
  invalid metadata nodes or null pixel buffers, returning an error code that indicates which check failed.
- [`plum_validate_palette_indexes`][validate-indexes] is a companion function used to validate the pixel buffer of an
  image using [indexed-color mode][indexed].
  This function checks all the pixel values for such an image, ensuring that they are no greater than the image's
  `max_palette_index` member; [`plum_validate_image`][validate] doesn't perform this check because it requires
  validating every single pixel of the image, and therefore it's much slower than all other checks combined.
  The function is a harmless no-op for images that don't use [indexed-color mode][indexed] at all (as well as for
  images that use a full 256-color palette), making it suitable for all images.
- [`plum_check_valid_image_size`][check-size] is specifically intended to mitigate the size overflow problem mentioned
  above.
  This function checks that the product of the dimensions given (together with the largest possible pixel size, which
  is `sizeof(uint64_t)`) won't overflow.
  While [`plum_validate_image`][validate] already performs this check on an existing [`struct plum_image`][image],
  this function is provided separately so that users can validate image dimensions before creating the struct.
- [`plum_check_limited_image_size`][check-limited] is a variant of [`plum_check_valid_image_size`][check-size] that
  also ensures that the product of all dimensions is no greater than the specified limit.
  This function is used internally by [`plum_load_image_limited`][load-limited] to enforce its pixel count limit (as
  described in [the previous section](#untrusted-image-files)), and it can be used by users for similar purposes.

**Warning:** these validations are only necessary for programs that allow other programs to send them image data (in
the form of a [`struct plum_image`][image]) directly.
This is something that might arise in a wrapper library, or any similar program that exposes an API for other code to
interact with it, but is very uncommon in end-user software.
Therefore, end-user programs will probably not need any of these functions, except perhaps for those that validate
image dimensions if the programs allow users to create a new image by specifying its dimensions.

* * *

Prev: [Supported file formats](formats.md)

Next: [Library versioning](version.md)

Up: [README](README.md)

[check-limited]: functions.md#plum_check_limited_image_size
[check-size]: functions.md#plum_check_valid_image_size
[errors]: constants.md#errors
[image]: structs.md#plum_image
[indexed]: colors.md#indexed-color-mode
[load]: functions.md#plum_load_image
[load-limited]: functions.md##plum_load_image_limited
[validate]: functions.md#plum_validate_image
[validate-indexes]: functions.md#plum_validate_palette_indexes
