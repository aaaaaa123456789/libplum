# Loading and storing modes

The [`plum_load_image`][load] and [`plum_store_image`][store] functions can reference data in a fixed-size memory
buffer, a variable-size memory buffer, a file, or anywhere the program can reach through a callback.
These possibilities are referred to as loading/storing modes in the documentation, and the one in use is determined by
the `size` argument to those functions.

- [Introduction](#introduction)
- [Fixed-size memory buffers](#fixed-size-memory-buffers)
- [Variable-size memory buffers](#variable-size-memory-buffers)
- [Accessing files](#accessing-files)
- [Data callbacks](#data-callbacks)

## Introduction

The [`plum_load_image`][load] and [`plum_store_image`][store] functions look like this:

``` c
struct plum_image * plum_load_image(const void * restrict buffer, size_t size,
                                    unsigned flags, unsigned * restrict error);
size_t plum_store_image(const struct plum_image * image, void * restrict buffer,
                        size_t size, unsigned * restrict error);
```

In both cases, the data that will be accessed (for reading or for writing, respectively) is determined by a pair of
arguments, `buffer` and `size`.
The `size` argument serves double duty: it determines which loading/storing mode will be used, and for fixed-size
buffers, it specifies the size of the buffer.
(The name `size` is just shorthand here; `loading_mode_and_size` would be too verbose to be comfortable.
Numeric arguments typically do signify actual sizes, after all; other loading/storing modes use special constants.)

Note that the chosen mode won't change how the functions behave (other than when loading or storing data).
In particular, since writing out data is the last step in [`plum_store_image`][store], if data generation fails, no
data will be written at all, regardless of the chosen mode.

## Fixed-size memory buffers

Fixed-size memory buffers are the simplest loading/storing mode of them all.

In this mode, the data is located in a memory buffer (when reading) or written to a buffer with a fixed capacity (when
writing).
The `size` argument is the size of that buffer, and the `buffer` argument is a pointer to that buffer.
(Note that the special constants used for the `size` argument in other modes use the highest possible `size_t` values,
so they are very unlikely to collide with an actual buffer size used by this mode.)

Note that, when loading an image, the exact size of the image data must be specified; the library will validate that
files don't contain excess invalid data.
(In other words, the `size` argument must be the actual size of the image data, not the size of the (possibly larger)
underlying buffer.)

When writing out image data, the `size` argument indicates the size of the buffer, and therefore the maximum amount of
data that can be written to the buffer.
If the generated image data doesn't fit in this size, then the function won't write any data at all and will fail with
[`PLUM_ERR_IMAGE_TOO_LARGE`][errors].
Otherwise, the data will be written at the beginning of the buffer, and [`plum_store_image`][store] will return the
actual number of bytes written, as usual.

## Variable-size memory buffers

When using fixed-size buffers isn't a solution (perhaps because the size of the data isn't known in advance),
variable-size memory buffers can be used.
These buffers are represented by a [`struct plum_buffer`][buffer] value:

``` c
struct plum_buffer {
  size_t size;
  void * data;
};
```

When using this mode, the `size` argument to the function must be set to [`PLUM_BUFFER`][constants], and the `buffer`
argument is a [`struct plum_buffer *`][buffer] pointing to the buffer data as shown above.

Loading data in this mode behaves exactly the same as with a [fixed-size memory buffer](#fixed-size-memory-buffers),
except that the `size` member of the struct is always interpreted literally and never as a
[special loading/storing mode constant][constants].
This mode is only provided for [`plum_load_image`][load] for completeness.

When writing out data using this mode, [`plum_store_image`][store] will allocate a buffer large enough to hold the
generated data with `malloc`, and set the struct's `data` member to a pointer to this buffer and its `size` member to
its size.
This buffer will be owned by the caller and must be released with `free`.
(If `free` isn't available, a special mode of [`plum_free`][free] with a `NULL` first argument may be used.)

If [`plum_store_image`][store] fails to allocate the buffer, it will fail with [`PLUM_ERR_OUT_OF_MEMORY`][errors].
If the function succeeds, it will return the number of bytes written, as usual; this will be the same as the value
written to the `size` member of the [`plum_buffer` struct][buffer].

**Warning:** [`plum_store_image`][store] will **not** write anything to the [`plum_buffer` struct][buffer] at
`*buffer` if it fails (i.e., if it returns 0).
The members of that struct will **not** be set to zero/`NULL`: they won't be modified at all.
If zeroing out the members on error is desired, initialize them to zero before calling [`plum_store_image`][store].

## Accessing files

The library can read image data directly from files, as well as write to them.
In order to do this, the `size` argument must be set to [`PLUM_FILENAME`][constants]; the `buffer` argument will be
a `const char *` containing a filename.
(While the `buffer` argument to [`plum_store_image`][store] isn't `const`-qualified, it is still treated as such in
this mode.)

The file will be opened with `fopen` (in binary mode) and data will be read and written using `fread` and `fwrite`.
If `fopen` fails, the function will fail with [`PLUM_ERR_FILE_INACCESSIBLE`][errors]; if reading or writing fails,
the function will fail with [`PLUM_ERR_FILE_ERROR`][errors].

**Warning:** if [`plum_store_image`][store] fails with [`PLUM_ERR_FILE_ERROR`][errors], partial data will have been
written to the file!

As always, [`plum_store_image`][store] will return the number of bytes written on success; this will be the size of
the file.
(Of course, [`plum_load_image`][load] will return the loaded image on success.)

Note: when using this mode, [`plum_load_image`][load] will simply read the entire file into a temporary buffer; the
function will _not_ read the data directly from the file as it is decoded.
Therefore, if the user has access to OS-specific functions, it is often faster to map the file to memory (using a
function like POSIX's `mmap` or Windows' `CreateFileMappingW`) and load it directly from memory as if it was a
[fixed-size memory buffer](#fixed-size-memory-buffers).

## Data callbacks

For cases where none of the above modes are helpful, it is possible to read or write data through a user-defined
callback function.
This allows users to implement any access mode they wish.

Callbacks are represented by a [`struct plum_callback`][callback] value:

``` c
struct plum_callback {
  int (* callback) (void * userdata, void * buffer, int size);
  void * userdata;
};
```

When using this mode, the `size` argument to the function must be set to [`PLUM_CALLBACK`][constants], and the
`buffer` argument is a [`const struct plum_callback *`][callback] pointing to the callback data as shown above.
(While the `buffer` argument to [`plum_store_image`][store] isn't `const`-qualified, it is still treated as such in
this mode.)

The [`plum_load_image`][load] function will repeatedly call the callback function to read data until the callback
returns an end of file; likewise, the [`plum_store_image`][store] function will repeatedly call the callback to write
data until it is done.
(There is no final call to indicate that [`plum_store_image`][store] is finished writing data; the user can make such
a call manually after the function returns if needed.)

The `userdata` argument to the callback function is the `userdata` member of the [`plum_callback` struct][callback];
this value isn't used by the [`plum_load_image`][load] and [`plum_store_image`][store] functions in any other way.
The `buffer` argument to the callback is the memory buffer: when called by [`plum_load_image`][load], the callback
must write the loaded data in that buffer, and when called by [`plum_store_image`][store], the callback must write the
data in that buffer wherever it shall.
The `size` argument is the size of the supplied buffer, and it will always be positive and no larger than `0x7fff`.

**Warning:** in C++ mode, the callback function must be marked `extern "C"` and it must not throw any exceptions.

If the callback is successful, it must return the number of bytes read or written.
It is always permissible for the callback to read or write fewer bytes than `size` indicates; the library will adjust
future calls accordingly if a smaller value is returned.
(On the other hand, the callback must not return a value larger than `size`.)

If the callback fails, it must return a negative value; this will cause the corresponding library function (i.e.,
[`plum_load_image`][load] or [`plum_store_image`][store]) to fail with [`PLUM_ERR_FILE_ERROR`][errors] without further
calls to the callback.

When loading data, a return value of 0 indicates end of file; the callback must return 0 when the data has finished
loading so that [`plum_load_image`][load] can process it.
When writing out data, a return value of 0 simply indicates that 0 bytes have been written and will cause
[`plum_store_image`][store] to retry.

As always, [`plum_store_image`][store] will return the number of bytes written on success; this will be the total
number of bytes written through the callback.
(Of course, [`plum_load_image`][load] will return the loaded image on success.)

* * *

Prev: [Alphabetical declaration list](alpha.md)

Next: [C++ helper methods](methods.md)

Up: [README](README.md)

[buffer]: structs.md#plum_buffer
[callback]: structs.md#plum_callback
[constants]: constants.md#special-loading-and-storing-modes
[errors]: constants.md#errors
[free]: functions.md#plum_free
[load]: functions.md#plum_load_image
[store]: functions.md#plum_store_image
