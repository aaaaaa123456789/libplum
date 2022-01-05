#ifndef PLUM_HEADER

#define PLUM_HEADER

#define PLUM_VERSION 30

#include <stddef.h>
#ifndef PLUM_NO_STDINT
#include <stdint.h>
#endif

#if !defined(__cplusplus) && (__STDC_VERSION__ >= 199901L)
/* C99 or later, not C++, we can use restrict, and check for VLAs and anonymous struct members (C11) */
/* indented preprocessor directives and // comments are also allowed here, but we'll avoid them for consistency */
#define PLUM_RESTRICT restrict
#define PLUM_ANON_MEMBERS (__STDC_VERSION__ >= 201112L)
/* protect against really broken preprocessor implementations */
#if !defined(__STDC_NO_VLA__) || !(__STDC_NO_VLA__ + 0)
#define PLUM_VLA_SUPPORT 1
#else
#define PLUM_VLA_SUPPORT 0
#endif
#elif defined(__cplusplus)
/* C++ allows anonymous unions as struct members, but not restrict or VLAs */
#define PLUM_RESTRICT
#define PLUM_ANON_MEMBERS 1
#define PLUM_VLA_SUPPORT 0
#else
/* C89 (or, if we're really unlucky, non-standard C), so don't use any "advanced" C features */
#define PLUM_RESTRICT
#define PLUM_ANON_MEMBERS 0
#define PLUM_VLA_SUPPORT 0
#endif

#ifdef PLUM_NO_ANON_MEMBERS
#undef PLUM_ANON_MEMBERS
#define PLUM_ANON_MEMBERS 0
#endif

#ifdef PLUM_NO_VLA
#undef PLUM_VLA_SUPPORT
#define PLUM_VLA_SUPPORT 0
#endif

#include "enum.h"
#include "color.h"
#include "pixeldata.h"
#include "struct.h"

/* keep declarations readable: redefine the "restrict" keyword, and undefine it later
   (note that, if this expands to "#define restrict restrict", that will NOT expand recursively) */
#define restrict PLUM_RESTRICT

#ifdef __cplusplus
extern "C" {
#endif

#include "func.h"

#ifdef __cplusplus
}
#endif

#undef restrict

/* if PLUM_UNPREFIXED_MACROS is defined, include shorter, unprefixed alternatives for some common macros */
/* this requires an explicit opt-in because it violates the principle of a library prefix as a namespace */
#ifdef PLUM_UNPREFIXED_MACROS
#include "unprefixed.h"
#endif

#endif
