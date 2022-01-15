# libplum

This is a C library designed to read and write common image file formats, and handle the resulting image data.
The library currently supports the BMP, GIF, PNG/APNG, JPEG and PNM (netpbm) formats; for more details, check the
[corresponding documentation page](docs/formats.md).

The main goal of the library is simplicity of use.
File formats are automatically detected, freeing the programmer from the burder of knowing in advance which formats to
support (or which one is currently being used by their users), and almost all format parameters are automatically
chosen by the library when generating an image file.
Reading and writing files (or data in memory) is done with a single function call and requires no setup.
The library has no dependencies besides the standard ISO C17 library functions, and therefore can be easily included
in any application.

Please make sure to check the [documentation](docs/README.md) for further information.
A good starting point is the [tutorial](docs/tutorial.md).

Everything in this repository is released to the public domain under [the Unlicense](LICENSE).

## Building and using the library

To build the source files for inclusion in other projects, use `make basefiles`; this simply requires common POSIX
command-line tools (like a Bash shell).
This will generate the `build/libplum.c` and `build/libplum.h` files.
To build a binary for the library (together with the files mentioned above), use `make`; this requires a C17 compiler,
and will generate the `build/libplum.so` file in addition to the source files.

To use the library, `#include "libplum.h"` from your code.
You may compile `libplum.c` along the rest of your code (provided you're using a C17-conformant compiler in your
project) or use the pre-built shared binary directly.

Releases contain the `libplum.c` and `libplum.h` files generated above, ready for inclusion.
Binaries are not included.

## More information

For further information, please check the [documentation](docs/README.md).

(Note that the documentation is included within the source tree, instead of using the wiki features, so that any
source archive will contain documentation suitable for that version of the library.
If you're using an older version of the library, please refer to that documentation if needed.)

## FAQ

**Q:** Does this library support the &lt;insert format here&gt; image format?

**A:** For the list of supported file formats, check [the relevant documentation page](docs/formats.md).

**Q:** Will &lt;currently unsupported format&gt; be supported?

**A:** If it has a public and established standard I can freely implement (i.e., without requiring a license), feel
free to link me to it and I may implement it.
Note that only true raster image formats are within scope: vector images (like SVG), containers (like ICO), or formats
that contain non-image data (like video files) will not be supported.

**Q:** What does the name "libplum" mean?

**A:** Nothing.
I was looking for a short word that could be easily pronounced to be used as a prefix for function and macro names,
and "plum" was the first one that came to mind.
It is intentionally meaningless and it is not an acronym or an abbreviation.
