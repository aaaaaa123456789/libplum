# Supported file formats

Each file format supported by the library has its own particularities in its implementation.
While the library attempts to smooth out the differences as much as possible, some minor differences remain, that can
be seen when an image is loaded in one format and stored in another.

All supported formats are documented here, along with their restrictions and some implementation notes.

- [Definitions](#definitions)
- [BMP](#bmp)
- [GIF](#gif)
- [PNG](#png)
- [APNG](#apng)
- [JPEG](#jpeg)
- [PNM](#pnm)

## Definitions

Some terms are used when describing many of the formats, and they are therefore defined here.

**True bit depth**

Actual bit depth of a component or set of components of an image, as calculated by the library.
It is always a value between 1 and 16 (or between 0 and 16 for the alpha component).
If the image has a [`PLUM_METADATA_COLOR_DEPTH`][metadata-constants] metadata node, then the bit depth for that
component is taken from the information in that node; otherwise, a default is used.

The true bit depth of a set of components is the greatest bit depth amongst those components; in particular, the true
bit depth for the image as a whole is the greatest bit depth amongst all four components (red, green, blue, alpha).
Note that the true bit depth of the whole image does **not** refer to the sum of the true bit depths of every
component: for example, an image using 8 bits for each of the red, green and blue components (and no transparency) has
a true bit depth of 8, not 24.

The exact rules for the true bit depth of a single component are:

- If the metadata node defines a non-zero color depth for that component, that value is used.
- If the component is a color component (i.e., red, green or blue), and the metadata node defines a depth of 0 for all
  three color components, but a non-zero depth for the gray component, the gray component's depth is used.
- In all cases above, if the defined depth is greater than 16, it is clamped to 16.
- If the metadata node defines a color depth of zero for that component, or if there is no color depth metadata node
  at all, then the bit width of that component in the image's [color format][color-formats] is used as a default.
- For the alpha component, if all colors in the image are fully opaque (in the image's palette if the image uses
  [indexed-color mode][indexed], or in the image's pixels otherwise), the bit depth is set to zero, regardless of the
  calculated depth above.

**Empty pixel**

An empty pixel is a pixel that is fully transparent, and whose RGB components match those of the image's background
color (as defined by a [`PLUM_METADATA_BACKGROUND`][metadata-constants] metadata node).
If the image doesn't have a defined background color, then an empty pixel is a fully transparent black pixel.

Empty pixels may be used to determine borders.
A portion of an image is considered to be an empty border if all pixels in that border (i.e., between the boundary of
the border and the edge of the image) are empty pixels.

## BMP

All variants of the Windows Bitmap (BMP) format are supported.
Other variants, such as OS/2 BMP files, will error out with [`PLUM_ERR_INVALID_FILE_FORMAT`][errors]; those variants
are extremely rare and most likely no longer in use.

BMP files only support a single frame; attempting to generate a file with two or more frames will fail with
[`PLUM_ERR_NO_MULTI_FRAME`][errors].
The maximum width and height for an image is `0x7fffffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

Images using [indexed-color mode][indexed] are fully supported, but they cannot contain transparency.
Therefore, if an image uses transparency, the file will be generated without a palette.

If an image has a [true bit depth](#definitions) of 8 or less and it doesn't use transparency, the usual RGB888 format
is used for output.
Otherwise, variable bit masks are used, with each component having the width determined by its true bit depth.
Variable bit width files are limited to a total of 32 bits per color, so if the sum of the true bit depths for all
components exceeds 32, they are proportionally reduced to fit.

## GIF

GIF (Graphics Interchange Format) files are supported as long as they contain all the palettes used by the image.
Images with implicit or "default" palettes aren't supported and will fail with [`PLUM_ERR_UNDEFINED_PALETTE`][errors].
(Likewise, image files that only define palettes and don't define any image frames will fail with
[`PLUM_ERR_NO_DATA`][errors].)

The maximum width and height for an image is `0xffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

An image will be loaded in [indexed-color mode][indexed] if it has a global palette, it has no per-frame palettes, and
it either uses no transparency or uses the same index for transparency in all frames.
In all other cases, the image will be loaded as if it didn't have a palette.

When generating a file, [indexed-color mode][indexed] images will use a global palette.
All other images will use per-frame palettes, and generating them will fail with [`PLUM_ERR_TOO_MANY_COLORS`][errors]
if a frame uses more than 256 distinct colors.

True frame dimensions will always be loaded into a [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node when
a file is loaded.
When generating a file, if the image contains such a metadata node, then for each frame, if the area outside the
defined region is made out of [empty pixels](#definitions), that frame will be encoded with the specified dimensions.
Otherwise (i.e., if the metadata node is absent or if the region outside the defined area does not contain only empty
pixels), if the image uses per-frame palettes, any frame of the image which has some border made of
[empty pixels](#definitions) will be encoded with reduced dimensions.

The library will ignore the image's version number (i.e., it will parse `GIF87a` and `GIF89a` files identically) and
will always generate `GIF89a` files.

The GIF format doesn't support the [`PLUM_DISPOSAL_REPLACE`][disposals] disposal method, or any of the combinations
including it; they will be converted to their non-replacing counterparts when generating an image file.

Valid [frame durations][durations] are between 0 and 655.35 seconds, in intervals of 0.01 seconds; values will be
rounded accordingly when generating a file.
When loading a file, a frame with a duration of 0 will be loaded as 1 nanosecond.
(While the specification implies that a duration of 0 indicates that the frame has infinite duration, common viewers
ignore it in this aspect.
The library's behavior is consistent with common viewers' behavior of displaying the frame as briefly as possible.)
When generating a file, a frame duration of 0 has no special meaning and is just converted like any other value.
(However, a duration of 0 will never be adjusted to compensate for cumulative rounding errors in previous frames.)

The common loop count extension is supported, and will be loaded as a [`PLUM_METADATA_LOOP_COUNT`][metadata-constants]
metadata node.
The maximum supported loop count is `0xffff`; higher values will be treated as 0 (i.e., infinity) when generating a
file.
Animations lacking this metadata node are not looped, and the corresponding files will be generated without the loop
count extension; if the loop count is 1, the extension won't be generated either.

## PNG

PNG (Portable Network Graphics) files are fully supported.
Any conformant PNG file should be loaded by the library without issues.
Images will be loaded in [indexed-color mode][indexed] if they use a palette.

The maximum width and height for an image is `0x7fffffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].
PNG files only support a single frame; the related format [APNG](#apng) must be used for multi-frame support.
(APNG files will be detected automatically on load.)
Attempting to generate a PNG file with two or more frames will fail with [`PLUM_ERR_NO_MULTI_FRAME`][errors].

The library only supports some of the standard ancillary chunks, namely `sBIT`, `bKGD` and `tRNS`.
These chunks will also be generated when generating a file if needed.
Other ancillary chunks, containing all sorts of metadata, will be ignored on load.
([APNG](#apng)'s ancillary chunks, `acTL`, `fcTL` and `fdAT`, will cause the image to be loaded as an [APNG](#apng)
file instead.)

The library supports reading PNG files using any color mode, but it will only generate files using modes 2 (RGB),
3 (indexed with palette) and 6 (RGB with alpha); it will never generate grayscale files.
For images that use [indexed-color mode][indexed], the library will always generate a mode 3 file; for other images,
it will use mode 6 if the image uses any transparency (i.e., the [true bit depth](#definitions) of the alpha component
is not zero), or mode 2 otherwise.

Some images using [indexed-color mode][indexed] have a background color set to an invalid index.
For compatibility, this will be ignored on load; the image will be loaded without a background color.
Likewise, when generating an image file, if the image uses [indexed-color mode][indexed] and its background color is
set to a color that isn't part of the palette, the image file will be generated without a background color.

The [true bit depth](#definitions) of the image will be used to select between 8-bit and 16-bit colors when generating
a PNG file: 8-bit components will be used when the true bit depth is 8 or less, and 16-bit components will be used
otherwise.
Nevertheless, an `sBIT` chunk will be generated, indicating the image components' [true bit depth](#definitions).

## APNG

APNG (animated PNG) files are fully supported, but treated as a format of their own.

APNG is a format that is superficially compatible with [PNG](#png) (i.e., it generates files that can be loaded by a
PNG loader without errors), but not actually conformant with the PNG specification, since it stores critical
information (namely, animation frames) in ancillary chunks.
This may cause any PNG editor that is unaware of APNG (such as `pngcrush`, at least at the time of writing) to destroy
all animation data when processing such a file.
Due to this incompatibility, the library treats APNG as an independent format, thus ensuring that it will never
accidentally generate an APNG file when the user actually requests a [PNG](#png) file.

All considerations listed for the [PNG](#png) format also apply to APNG, except that APNG supports multi-frame images.

If an image's `type` field is set to [`PLUM_IMAGE_APNG`][image-types], the library will always generate an APNG file
for it, even if it only has one frame.
The maximum number of frames in an APNG file is `0x40000000`; larger numbers will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

APNG stores frame durations in seconds, as a quotient of two 16-bit numbers; the maximum value that can be specified
this way is 65,535 seconds.
When generating a file, durations stored in a [`PLUM_METADATA_FRAME_DURATION`][metadata-constants] metadata node will
be rounded to the nearest irreducible fraction that best approximates that value; durations above 65,535,000,000,000
nanoseconds will be clamped to that value.

The first frame of an APNG file may be excluded from the animation and only used to be statically displayed as a PNG
file.
The library will load an excluded first frame with a duration of 0; frames that are included in the animation, but
have their durations set to 0, will be loaded with a duration of 1 nanosecond instead.
(This is consistent with the APNG specification requiring engines to "render the next frame as quickly as possible".)
Likewise, when generating a file, if the first frame's duration is 0, it will be excluded from the animation.
(Since other frames cannot be excluded from the animation, a duration of 0 has no special meaning for other frames and
will be converted like any other value.
However, a duration of 0 will never be adjusted to compensate for cumulative rounding errors in previous frames.)

The maximum loop count supported in a [`PLUM_METADATA_LOOP_COUNT`][metadata-constants] metadata node is `0x7fffffff`;
higher values will be treated as 0 (i.e., infinity) when generating a file.
Animations lacking this metadata node will have a loop count of 1.

True frame dimensions will always be loaded into a [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node when
a file is loaded.
When generating a file, if the image contains such a metadata node, then for each frame other than the first, if the
area outside the defined region is made out of [empty pixels](#definitions), that frame will be encoded with the
specified dimensions.
Otherwise (i.e., if the metadata node is absent, if the region outside the defined area does not contain only empty
pixels, or if the frame in question is the first frame of the image), the corresponding frame will be encoded as a
full-size frame.
If reducing frames this way removes all transparent pixels from the image, the image will be considered to not use
transparency for the purposes of selecting the mode to use.

## JPEG

All JPEG (Joint Photographers Expert Group) standard formats are supported.
This includes rarely-used forms described in the standard, such as hierarchical files, arithmetic coding, or 12-bit
data.

Although the library supports loading all sorts of uncommon JPEG files, it will always generate baseline JPEG/JFIF
files (8-bit precision, Huffman coding, YCbCr color space, 4:2:0 chroma subsampling).
This aims to increase the compatibility of said files, since many decoders don't support uncommon JPEG formats.

The JPEG specification doesn't define the color formats an image can use.
Instead, it expects applications to agree on the meaning of component IDs.
Therefore, any JPEG implementation has to guess at the color space used by any image it loads.
This library attempts to guess following the rules that several common implementations follow, but if it can't, it
will fail with [`PLUM_ERR_INVALID_FILE_FORMAT`][errors].
There is intentionally no fallback format if all attempts to guess at a color format fail.

Some JPEG encoders will use non-standard component IDs and describe the color space using (also non-standard) ICC
metadata containing a color profile.
This library doesn't handle color profiles in any way; those images may be loaded incorrectly or fail to load.
However, this is fortunately a rare occurrence.

JPEG files only support a single frame; attempting to generate a file with two or more frames will fail with
[`PLUM_ERR_NO_MULTI_FRAME`][errors].
The maximum width and height for an image is `0xffff`; larger dimensions will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].

JPEG compression is lossy: in general, it is not possible to perfectly reconstruct an image that has been encoded as
JPEG.
Since generating a file implies reencoding it, loading a JPEG image file with this library and then storing it again
will necessarily reencode the image, adding to the accumulated error.

The degree of information loss that is acceptable when encoding an image is generally described in terms of "quality",
a percentage that measures the error introduced by the process, where 100% quality minimizes the error and 0%
maximizes it.
This library doesn't allow the user to select a quality when generating a file, since the goal is to provide a unified
interface for all image file formats.
Instead, it will automatically select a quality based on the dimensions and [true bit depths](#definitions) of the
image.

While there are some JPEG color formats that support transparency (like PhotoYCCA), these are extremely rare and not
supported by most encoders.
Therefore, while the library may load those files, it will never generate an alpha channel when generating a JPEG
file; this is part of the information that is lost when encoding a file as JPEG.

## PNM

Netpbm defines a family of formats, known collectively as PNM (portable anymap; the N stands for 'any').
All of those formats are supported by this library.

The PNM family of formats includes text and binary variants of the PBM (black/white bitmap), PGM (grayscale) and PPM
(RGB color) formats.
It also includes the newer (always binary) PAM format; the library supports this format as long as the tuple type is
one of the standard tuple types that describe color data.

Binary formats may contain multiple frames, concatenated after one another.
The library supports this; it also supports, as a common extension, mixing different PNM formats when concatenating
them.
(Text-based formats are defined to accept and discard all excess data at the end, and thus a text-based frame will
always be the last frame of the image.
However, the library does accept concatenating a text-based frame after any number of binary frames.)

When loading a multi-frame image, if the frames are of different sizes, the image's width and height will be set to
the maximal values for each dimension across all images.
Excess pixels below and to the right of each frame's actual dimensions will be set to fully-transparent black.
The [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node will reflect the true dimensions of each frame in
the image; since all frames begin at the top left corner of the image, the `top` and `left` members of the
corresponding [`struct plum_rectangle`][rectangle] elements will be set to zero.

When storing an image, the library may generate a binary PPM image or a PAM image.
This will depend on whether the image has any transparency and on whether it contains a
[`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node.
(The library will never generate PBM, PGM, or text-based PPM images.)
This will also determine the dimensions of the generated image frames.

If the image has only one frame, the generated dimensions will always match the image dimensions.
If that frame uses transparency (i.e., the alpha channel's [true bit depth](#definitions) is not zero), the library
will generate a PAM file; otherwise, it will generate a binary PPM file.
The [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node has no effect in this case.
(Note that this isn't truly a special case: it results from applying the rules below for multi-frame images.
However, it is highlighted for simplicity.)

If a multi-frame image has a [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node, then the frame dimensions
are adjusted as follows:

1. For each frame, the top left corner of the frame area is placed at the origin, while keeping the bottom right
   corner in place.
   In practice, this means setting `width` to `width + left` and `height` to `height + top`, while resetting `top` and
   `left` to zero.
   (Note that these changes, as well as all subsequent changes, are applied on a copy of the frame area metadata.
   The data in the image is not modified.)
2. Since there is no separate way to specify the canvas size in a PNM file, if none of the frames has the same width
   as the image, the first frame's width is set to the image's width.
   The same is done for the height.
3. For each frame, if the frame only contains [empty pixels](#definitions) outside of the defined region, the frame
   area is considered valid.
   Otherwise, the frame dimensions for that frame are reset to the image's dimensions.

If the resulting image doesn't use transparency in the defined regions, the library will generate a binary PPM file.
Otherwise, it will generate a PAM file.

If a multi-frame image does _not_ have a [`PLUM_METADATA_FRAME_AREA`][metadata-constants] metadata node, then the
frame dimensions are by default equal to the size of the image.
The resulting file will be a PAM file if the image uses transparency in any frame (i.e., if the alpha channel's
[true bit depth](#definitions) is not zero), or a binary PPM file otherwise.
However, if the image uses transparency, the library will attempt to remove that transparency if possible.
If **all** of the following conditions are true, the frame dimensions for each frame will be adjusted so that they
exclude the [empty borders](#definition) to the right and the bottom of each frame, causing a binary PPM file to be
generated instead of a PAM file:

- The only transparent pixels in each frame are [empty pixels](#definitions) to the right and at the bottom of the
  image.
- Those [empty borders](#definitions) are strips that contain nothing but [empty pixels](#definitions); all other
  pixels in each frame are fully opaque.
- At least one frame would have a width equal to the image's width, and at least one frame would have a height equal
  to the image's height.
- The top left corner of each frame is an opaque pixel.
  (In other words, no frame would have a width or height of zero.)

These conditions are mostly intended to ensure that multi-frame images that were loaded from PBM, PGM or PPM files can
be stored as PPM files instead of PAM files, since the transparent borders in those cases are artificially introduced
by the library to pad all frames to a common width and height.

The library supports reading PNM files with their maximum value set to any number between 1 and 65,535, even if it
isn't one less than a power of two.
However, it will generate files with their maximum value set according to the image's [true bit depth](#definitions),
which will always be one less than the corresponding power of two.
(Note: for PAM images with their tuple type set to `BLACKANDWHITE` or `BLACKANDWHITE_ALPHA`, the only sensible maximum
value is 1.
Other maximum values are not supported and will fail with [`PLUM_ERR_INVALID_FILE_FORMAT`][errors].)

While PNM files can contain multiple frames, they cannot contain animations.
All [animation-related metadata][animation] will be ignored when generating a PNM file.

PNM files don't support palettes; [indexed-color mode images][indexed] will be converted when a PNM file is generated.

* * *

Prev: [C++ helper methods](methods.md)

Next: [Handling untrusted input](untrusted.md)

Up: [README](README.md)

[animation]: metadata.md#animation-metadata-types
[color-formats]: colors.md#formats
[disposals]: constants.md#frame-disposal-methods
[durations]: metadata.md#plum_metadata_frame_duration
[errors]: constants.md#errors
[image-types]: constants.md#image-types
[indexed]: colors.md#indexed-color-mode
[metadata-constants]: constants.md#metadata-node-types
[rectangle]: structs.md#plum_rectangle
