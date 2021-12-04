# Conventions and limitations

- [Conventions](#conventions)
- [Limitations](#limitations)

## Conventions

The library follows a number of conventions to make it easier to use.

All public identifiers (functions, macros, etc.) begin with `plum_` or `PLUM_`.
Macros and constants are all uppercase; functions and `struct` tags are all lowercase.
In all cases, words are separated by underscores and capitalization is not mixed.
There is a number of macros that [don't follow the prefix rules][unprefixed], but those macros aren't included by
default, and the user has to explicitly enable them with a `#define` in order to use them.

Even though the `const` qualifier is shallow in C (i.e., for some `struct T`, a `const struct T *` argument will not
allow direct modification of the members of the `struct`, but it _will_ allow modifying the data those members point
to), all functions that accept `const` pointers treat the qualifier as deep.
For instance, the [`plum_store_image`][store] function, whose first argument is a `const struct plum_image *`, will
not modify any of the image's data.

All references to colors use the names listed in the [Color formats][colors] page.
In particular, all macros that manipulate color values are suffixed with `16`, `32`, `32X` or `64` to indicate the
color format they operate on.
Members and accessors that only depend on the data type, like the `data32` member of the [`struct plum_image`][image]
type, use `32` for both 32-bit formats, since they both use `uint32_t` values.

All functions that take `restrict` pointer arguments use the `restrict` keyword in the documentation.
If the library header is included in a C89 source file, as determined by testing the `__STDC_VERSION__` standard
definition, the `restrict` keyword will be taken out through a macro, since that keyword doesn't exist in C89.
However, since the library does expect `restrict` semantics, the keyword is still included in the documentation.

## Limitations

The library targets ISO C17, and it should work in almost any conformant implementation.
However, it does make some basic assumptions about the environment.
While these assumptions are almost certain to be true in any system from the last few decades, they are documented
here for completeness.

- The system uses 8-bit bytes (i.e., `CHAR_BIT` is 8).
  This is enforced by the usage of the `uint8_t` type.
- The system uses two's complement for signed numbers.
  This is enforced by the usage of the `int16_t` type.
- All data pointers ("object pointers" in the standard's parlance) have the same representation; in other words, you
  can copy one pointer into another and access it that way (assuming the pointer is properly aligned).
  This in turn implies that, if a union contains pointers as members, a value assigned to one of those members can be
  read back through any other member.
- Memory allocation is available.
  If `malloc` always returns a null pointer, many functions will fail with an error.
- The stack is large enough to hold the library's temporary arrays.
  The compiler should be able to determine the maximum stack usage of the library, as it doesn't use any dynamic stack
  allocation (like VLAs or `alloca`) and there are no recursive functions that allocate large automatic arrays.
  While it isn't particularly large for a common desktop machine, systems with very small stacks (like 32 KB) might
  run into stack overflows.

In particular, the library doesn't make any assumptions about endianness, floating point format, system locale,
character encoding or alignment requirements.

* * *

Back to [README](README.md).

[colors]: colors.md
[image]: #
[store]: #
[unprefixed]: #
