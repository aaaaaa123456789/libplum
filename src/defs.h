#ifndef PLUM_DEFS

#define PLUM_DEFS

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

#endif
