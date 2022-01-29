# libplum fuzzer setup

This branch contains the setup used to fuzz the library.

The code in this branch includes `fuzz.c`, which is the fuzzing program (which mutates the input in various ways to
create valid inputs of multiple forms for the library while maximizing coverage) and `fuzztest.c`, a program that
manually invokes the code in `fuzz.c` for debugging and validation.

Thanks to [nyanpasu64](https://github.com/nyanpasu64) for their help setting up the initial libfuzzer setup, still
available on their fork of the library, from which this setup has been derived.

All code in this branch, like the main branch, is released to the public domain under [the Unlicense](LICENSE).

## Requirements

The code in this branch targets clang, and a modern enough version of clang (preferrably 11+) is required to compile
it.
(Set your `CLANG` environment variable to point to your clang binary if it's not `clang`.)

Fuzzing with AFL++ requires having AFL++ installed; the default compiler for the AFL++ binaries is `afl-clang-lto`.
(Set your `AFLCC` environment variable to change this.)

A copy of the library is also needed for building, of course.
Place the `libplum.c` and `libplum.h` files for the version of the library you're fuzzing alongside the `fuzz.c` and
`fuzztest.c` files for building.
These files have already been added to `.gitignore` for convenience.

## Building and using the fuzzers

To build the libfuzzer-enabled binaries, as well as `fuzztest` and `fasttest` (which are standalone testing programs
built from `fuzztest.c`, in debug and release mode respectively, used to test individual inputs), simply invoke the
`make` command.
This will output the `fuzz` and `fuzz32` binaries, which are linked against libfuzzer, as well as the binaries
mentioned above.
(`fuzz32` is built with the `-m32` switch, allowing testing of 32-bit binaries in a 64-bit platform.)
Check the [libfuzzer documentation](https://llvm.org/docs/LibFuzzer.html) for further information.

To build `covtest`, the binary used for coverage measurements, invoke `make covtest`.
Check [LLVM's source-based code coverage documentation](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) for
information on how to use this binary.

To build the binaries for use with AFL++, invoke `make afl`; this will output a number of binaries in the `afl`
subdirectory, built under different configurations, ready for invocation by `afl-fuzz`.
Check the [AFL++ documentation](https://github.com/AFLplusplus/AFLplusplus/tree/stable/docs) for further information.

As usual, `make clean` will delete all built binaries (and the entirety of the `afl` subdirectory).
