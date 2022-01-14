#ifndef PLUM_DEFS

#define PLUM_DEFS

#if defined(PLUM_NO_STDINT) || defined(PLUM_NO_ANON_MEMBERS) || defined(PLUM_NO_VLA)
  #error libplum feature-test macros must not be defined when compiling the library.
#elif defined(__cplusplus)
  #error libplum cannot be compiled with a C++ compiler.
#elif __STDC_VERSION__ < 201710L
  #error libplum requires C17 or later.
#endif

#ifdef noreturn
  #undef noreturn
#endif
#define noreturn _Noreturn void

#ifndef alignas
  #define alignas _Alignas
#endif

#ifdef PLUM_DEBUG
  #define internal
#else
  #define internal static
#endif

#define bytematch(address, ...) (!memcmp((address), (unsigned char []) {__VA_ARGS__}, sizeof (unsigned char []) {__VA_ARGS__}))
#define byteoutput(context, ...) (memcpy(append_output_node((context), sizeof (unsigned char []) {__VA_ARGS__}), \
                                         (unsigned char []) {__VA_ARGS__}, sizeof (unsigned char []) {__VA_ARGS__}))
#define bytewrite(address, ...) (memcpy(address, (unsigned char []) {__VA_ARGS__}, sizeof (unsigned char []) {__VA_ARGS__}), \
                                 sizeof (unsigned char []) {__VA_ARGS__})

#endif
