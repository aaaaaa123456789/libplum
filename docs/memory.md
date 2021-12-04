# Memory management

The library will manage the memory of all images it creates through the [constructor functions][constructors].
Since this functionality is already internally available, it is also exposed through the API for users to take
advantage of it.
Therefore, an image can have any number of memory allocations associated to it, that will be deallocated when the
image is destroyed through the [`plum_destroy_image`][destroy] function.
This helps avoid memory leaks and makes image deallocation much simpler.

Memory allocations associated to an image are performed using the [`plum_malloc`][malloc], [`plum_calloc`][calloc] and
[`plum_realloc`][realloc] functions; these functions take an additional argument, which is a pointer to a
[`struct plum_image`][image] value.
They work like their `<stdlib.h>` counterparts, but they also associate the memory they allocate to the image object
they are passed; that memory can later be freed explicitly by calling [`plum_free`][free], or it can be automatically
collected when the image is destroyed through [`plum_image_destroy`][destroy].
(Note: the [`plum_allocate_metadata`][allocate-metadata] function will allocate memory in a manner similar to
[`plum_malloc`][malloc], and thus everything mentioned in this page applies to that memory too.)

The [image constructor functions][constructors] will allocate memory for the [`struct plum_image`][image] itself, as
well as all the buffers it will contain, as if it was allocated by the memory management functions too; therefore,
that memory will also be released when [`plum_image_destroy`][destroy] is called.
However, the memory management functions can be used with any image, even if it has been statically initialized; in
that case, while [`plum_image_destroy`][destroy] will not release the memory for the image itself (as it hasn't been
allocated through these functions), it will release any memory allocated through these functions later on and
associated to that image.
Therefore, it is always safe (and often necessary) to call [`plum_image_destroy`][destroy] on an image that will no
longer be used, regardless of how it was created.

It is obviously not possible for a user to call [`plum_malloc`][malloc] to allocate the [`struct plum_image`][image]
itself, since [`plum_malloc`][malloc] requires a pointer to the image that the memory will be associated to.
Therefore, the [`plum_new_image`][new] function is made available in the API, which will allocate memory for an image
struct as if by [`plum_malloc`][malloc] (by using an equivalent internal mechanism) and associate it to itself.
The other [constructor functions][constructors] operate in a similar way when allocating the structs they return.
Note that, for images not created through these functions, the `allocator` member must be initialized to a null
pointer, since that is the initial state expected by the internal allocation functions.
(This member must not be otherwise modified by the application.)

Note that, while images created by [constructor functions][constructors] are allocated as if by
[`plum_malloc`][malloc], [`plum_free`][free] must not be called on the images themselves.
Calling `plum_free(image, image)` will eventually result in undefined behavior; those images must be deallocated by
calling [`plum_destroy_image`][destroy], which will also release all memory associated to them.

On the other hand, the individual buffers allocated when constructing an image _can_ be freed with [`plum_free`][free]
(which might be useful if they are being replaced with other data).
These buffers are the `data` member, the `palette` member (when not `NULL`), and each individual metadata node.

* * *

Prev: [Data structures](structs.md)

Next: [Metadata](metadata.md)

Up: [README](README.md)

[allocate-metadata]: #
[calloc]: #
[constructors]: #
[destroy]: #
[free]: #
[image]: structs.md#plum_image
[malloc]: #
[new]: #
[realloc]: #
