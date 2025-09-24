# Building and including the library

The library has been designed to be easy to include in any C project (and C++ by extension, as long as a C compiler is
also available).
It has no dependencies besides what the C standard requires; the source of the library targets ISO C17.

- [Building the library](#building-the-library)
- [Using and including the library](#using-and-including-the-library)

## Building the library

The build process will generate three files in the `build` directory (creating it if needed): `libplum.c`, `libplum.h`
and the library's shared binary (`libplum.so` by default).
To build the library, you will need GNU `make` and a Bash shell; simply run `make` (or `gmake` if that is its name).
The default configuration assumes a GCC-like compiler; for a completely different compiler (like MSVC), the flags (and
perhaps the GNUmakefile itself) will most likely need customization.
The following variables can be specified in the `make` command:

- `CC`: C compiler binary; will be read from the environment if it exists.
  Defaults to `gcc` otherwise.
- `OUTPUT`: name of the output binary; will be read from the environment if it exists.
  Defaults to `libplum.so`; Windows users will probably want to set this to `libplum.dll` instead.
- `CFLAGS`: contains the flags that will be given to the compiler, other than CPU-specific optimization flags.
- `OPTFLAGS`: contains CPU-specific optimization flags; they default to `-march=native -mtune=native`.
  Users that want to build the library for redistribution might want to specify an empty value for this variable.

The build process will first generate the `libplum.c` and `libplum.h` files through a Bash script that merges all the
source files together (`libplum.c` contains all of the source code, and thus it is independent from `libplum.h`).
The `libplum.c` file is the file that is actually compiled, and the `libplum.h` file is the header that should be
included in programs that use the library.

It is therefore possible to generate only these source files, to be later compiled manually with any other compiler.
This can be done directly with the `make basefiles` command.
The `libplum.c` file can be compiled with any C17-conformant compiler; it has no dependencies and it can be built by
itself, or it can be built together with other source files for static inclusion into a larger program.

The `make debug` command will build a debug build of the library.
This is functionally identical to the release version (the library contains no debug-only code), but it will contain
debug information, it will disable optimizations, and it will expose all internal symbols.
Therefore, this is only meant to debug the library itself, not programs that use it; it is **strongly** recommended
that programs using the library use a release build, even if those programs themselves are being debugged.
The `debug` target will generate a `libplum-debug.so` file, and it uses the `DEBUGFLAGS` variable to pass extra flags
to the compiler; it will ignore all other variables other than `CC`.

As it is conventional, `make clean` will delete the `build` directory and all files within it.

The library is intentionally compiled using GCC's default warning flags.
At the time of writing, using GCC 11, that produces no warnings.
Other compilers might have different default warning settings and produce a number of warnings when building the
library; likewise, GCC will produce a number of warnings if some warning flags (like `-Wall -Wextra`) are given.
Warnings that are already known have been determined to be spurious and don't signal any actual bug in the code.

## Using and including the library

The library can be included in two forms: source or binary.

To include the library in source form, simply copy the `libplum.c` file into your program and compile it alongside the
rest of your program.
This is the recommended way if your compiler is C17-conformant, as it allows the program to ensure it is using the
correct version of the library, since it will become part of the program's source code.

To include the library in binary form, link against the shared binary (`libplum.so` in POSIX environments).

To use the library in your source code, `#include` the header file (`libplum.h`).
This file will work with any standard version of C and C++, as long as exact-width integer types (like `uint32_t`) are
available.
Those types are available in all versions of C since C99 and in all versions of C++.

The library can make available some macros that aren't prefixed with `PLUM_` to make your code more readable.
These macros are disabled by default; to enable them, add a `#define PLUM_UNPREFIXED_MACROS` definition before
including the library header.

If your program is using C89, your compiler might still provide the `<stdint.h>` header; in that case, you can include
the library header directly.
Otherwise, add a `#define PLUM_NO_STDINT` definition and `typedef` definitions for the `uint8_t`, `uint16_t`,
`uint32_t` and `uint64_t` types before including the library header.
(The library **requires** 64-bit integer types: if you cannot define the `uint64_t` type, you cannot use the library.)
This can be done in a separate header, like so:

``` c
/* C89-only helper to substitute the missing <stdint.h> header */
#ifndef HELPER_LIBPLUM_H

#define HELPER_LIBPLUM_H

#define PLUM_NO_STDINT

/* sample definitions for a hypothetical environment; read your compiler's
   documentation to find out which types to use for these definitions      */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
typedef __u64 uint64_t; /* unsigned long long is not available in C89;
                           the compiler will often provide an alternative */

/* #define PLUM_UNPREFIXED_MACROS (if you want those macros) */
#include "libplum.h"

#endif
```

* * *

Prev: [Introduction and tutorial](tutorial.md)

Next: [Conventions and limitations](conventions.md)

Up: [README](README.md)
