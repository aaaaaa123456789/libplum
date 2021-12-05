# Metadata

Metadata is used to augment an image's information, storing important information about the image that isn't the
actual pixel data.
Since the library intends to present a unified interface for all image formats and a simple API, the amount of data
stored as metadata is minimal, only representing data that image formats may need for their basic functionality.

- [Basics](#basics)
- [Generic metadata types](#generic-metadata-types)
    - [`PLUM_METADATA_COLOR_DEPTH`](#plum_metadata_color_depth)
    - [`PLUM_METADATA_BACKGROUND`](#plum_metadata_background)
- [Animation metadata types](#animation-metadata-types)
    - [`PLUM_METADATA_LOOP_COUNT`](#plum_metadata_loop_count)
    - [`PLUM_METADATA_FRAME_DURATION`](#plum_metadata_frame_duration)
    - [`PLUM_METADATA_FRAME_DISPOSAL`](#plum_metadata_frame_disposal)

## Basics

Metadata is stored in [`struct plum_metadata`][struct] nodes, in a linked list.
The order of the nodes isn't meaningful; images loaded with [`plum_image_load`][load] may load their metadata in any
order.

The type of metadata represented by each node is given by its `type` member.
**Negative types are reserved for the application**, and applications may use them in any way they wish; the library
will not process metadata with negative types.
Positive types correspond to [metadata constants][constants], and will be described in this document; they can only
appear once in the image, and having two of the same positive-type metadata is an error.

The special constant `PLUM_METADATA_NONE`, with a value of zero, represents an unused metadata node; this can be used
to "hide" a node without actually removing it from the linked list and freeing its memory.
An image may have any number of `PLUM_METADATA_NONE` metadata nodes, and these nodes will be ignored when processing
metadata.
(Nevertheless, all metadata nodes will be copied by [`plum_copy_image`][copy], including nodes with negative types and
`PLUM_METADATA_NONE` nodes; their order will also be preserved.)
Using a positive type other than the ones defined in the [metadata constants][constants] and described here is an
error.

In all cases, [`plum_store_image`][store] will attempt to store the metadata types defined here, or otherwise use them
to define the output format's parameters.
(Application-defined negative-type metadata nodes will never be stored.)
However, this is done in a best-effort basis: if an image format doesn't support a particular element of metadata, it
will be silently ignored.

## Generic metadata types

These metadata elements are applicable to all images, and they contain data that will be used whenever the underlying
image format supports it.

### `PLUM_METADATA_COLOR_DEPTH`

This metadata node determines the real precision of all the component channels (red, green, blue, alpha) in the image.
It contains between three and five `uint8_t` values; [`plum_load_image`][load] will always generate all five, but
it is acceptable for users to generate `PLUM_METADATA_COLOR_DEPTH` nodes with a size of three or four, in which case
the missing components will be taken to be zero.

The five values, in order, represent the bit depth of the red, green, blue, alpha and grayscale channels.
While the library doesn't have any explicit support for grayscale, whenever a grayscale image is loaded,
[`plum_image_load`][load] will set the depth of the grayscale to a non-zero value (and the depth of the red, green and
blue channels to zero) to indicate that the original image contained only grayscale data.
In other cases, the grayscale channel will be set to zero depth and the red, green and blue channels will show their
proper bit depths.
The alpha channel will have a non-zero depth if the original image had transparency data, or zero otherwise.

The [`plum_image_store`][store] function will use this metadata node, if present, to determine the actual precision to
use for each channel when storing the image.
This will be adjusted according to what the actual image format allows.
If the red, green and blue channels are all set to zero depth, the grayscale depth will be used for all three;
otherwise, the grayscale depth will be ignored.

Channels set to zero depth will use the default bit depth for the image's [color format][formats].
In particular, this means that setting a channel's depth to zero won't cause that channel to be omitted.
If this metadata node is missing, all depths will be assumed to be zero and therefore derived from the defaults.

### `PLUM_METADATA_BACKGROUND`

This metadata node indicates the image's background color.
This will only be stored in image formats that support background colors; [`plum_load_image`][load] will likewise load
it when the image format supports background colors and the image defines one.

The format for this value is a single color value, in the format determined by the image's [color format][formats];
the size must be the size of that value.
(Note that this is a color value, not a color index; even for images using [indexed-color mode][indexed], the
background color must be a color value, and not a `uint8_t` palette index.)

## Animation metadata types

These metadata types describe animations.

Some image formats support animated images; animations are comprised of a number of frames, which are displayed for a
specific period of time and rendered on top of one another in a specific way.
These metadata nodes contain the parameters that define those animations.

### `PLUM_METADATA_LOOP_COUNT`

This metadata node determines how many times an animation will loop.
A value of 1 indicates that the animation doesn't loop; a higher value indicates that the animation will be repeated
that number of times.
A loop count of 0 indicates that the animation is repeated forever.

The [`plum_load_image`][load] function will always load this metadata node when it is available; likewise, the
[`plum_store_image`][store] function will always store it when supported.
If this node is missing, the loop count is assumed to be 1.
If the loop count is too large for the chosen image format, it will be converted to 0 (i.e., infinite) when storing
the image.

This node contains a single `uint32_t` value, and its size must be `sizeof(uint32_t)`.

### `PLUM_METADATA_FRAME_DURATION`

This metadata node determines how long each frame will be displayed.
Durations are measured in nanoseconds; a duration of 0 has the special meaning of indicating that the frame is not
part of the animation at all.
(Not all image formats support this.
To explicitly indicate that a frame is part of the animation, but must be skipped instantly, use a duration of 1
nanosecond; [`plum_load_image`][load] follows this convention when loading frame durations.)

Note that frame durations may be rounded or clamped by [`plum_store_image`][store] to fit the limitations of the
chosen image format.

This node contains an array of `uint64_t` values; it may contain any number of them.
(The [`plum_load_image`][load] function will load exactly as many as the image has frames.)
If there are excess values, they are ignored; if there are too few values, the missing values are taken to be 0.
If this node is missing, all frame durations are taken to be 0.

The size of this node indicates the size of the array.
It is an error for the size to not be a multiple of `sizeof(uint64_t)`.

### `PLUM_METADATA_FRAME_DISPOSAL`

This metadata node determines what will happen to a frame once its display time expires.
Each frame is assigned a disposal method, which must be one of the [frame disposal constants][disposal-constants].

This node contains an array of `uint8_t` values; it may contain any number of them, as determined by the node's size.
(Recall that `sizeof(uint8_t)` is required to be 1.)
The [`plum_load_image`][load] function will load exactly as many as there are frames.
The [`plum_store_image`][store] function will treat missing values as equal to the default disposal method (which is
`PLUM_DISPOSAL_NONE`), and ignore excess values; if the node is missing entirely, all disposal methods will be treated
as equal to `PLUM_DISPOSAL_NONE`.

* * *

Prev: [Memory management](memory.md)

Next: [Function reference](functions.md)

Up: [README](README.md)

[constants]: #
[copy]: functions.md#plum_copy_image
[disposal-constants]: #
[formats]: colors.md
[indexed]: colors.md#indexed-color-mode
[load]: #
[store]: #
[struct]: structs.md#plum_metadata
