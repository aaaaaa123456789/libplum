# Library versioning

In order to prevent breaking user code, the library defines and exposes a version number, which users may check for
compatibility reasons.

- [Versioning scheme](#versioning-scheme)
- [Version constants](#version-constants)

## Versioning scheme

This library uses a simplified versioning scheme based on semantic versioning.
Unlike semantic versioning, which defines three components for a version number, this library only has two: major and
minor.

The **major** version changes whenever there is a breaking change in the library.
Major version 0 (i.e., version 0.x) is used to indicate that the library isn't stable yet, and interfaces may change
between releases.
Major version 1 will be the first stable release, and further major versions will indicate breaking changes in the
library's interface or functionality.

The **minor** version changes with every release that isn't a major release; major releases reset it back to zero.
For major version 0, while the library isn't stable, all releases are minor versions, and may contain breaking
changes; after the first stable release (i.e., version 1.0), minor version updates imply fully-backwards-compatible
changes (at least as far as documented behavior is involved).

A version of the library may be in development, a release candidate, or a release.
In development is the state of a file that hasn't been released at all, like a repository snapshot; it is obviously
not fully tested, and may contain any number of bugs and partially-developed features.
Release candidates may be released before a proper release for wider testing of new features or bug fixes, and they
will be designated as such in the release information.

## Version constants

The library's version number can be checked by the [`plum_get_version_number`][function] function, and the version
number for the included library header can be checked by the [`PLUM_VERSION`][macro] macro; these two values should
normally match, or at least indicate the same major version of the library.

This value is comprised of three decimal fields (i.e., fields determined by decimal digits).

The last digit of the version number indicates what kind of version it is: 0 indicates a version in development, 9
indicates a release, and any other digit indicates a release candidate (1 for release candidate 1, 2 for release
candidate 2, and so on).
The next three digits indicate the minor version, and all upper digits indicate the major version.
Note that development versions of the library will be numbered after the upcoming version, not the last released one,
to ensure that they appear more recent than the last release.

For example, a value of `10029` indicates release version 1.2, and `10021` would have been the first release candidate
towards (and therefore before) that release.
The development version would be `10030` (or `20000` if a breaking change is already known to exist), to be eventually
released as version 1.3 (or 2.0).
Versions 0.x of the library lack the upper digits: for instance, a value of `19` indicates release version 0.1, and
`20` is the development version towards version 0.2.

* * *

Prev: [Supported file formats](formats.md)

Up: [README](README.md)

[function]: functions.md#plum_get_version_number
[macro]: macros.md#feature-test-macros
