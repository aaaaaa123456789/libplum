struct plum_buffer {
  size_t size;
  void * data;
};

#ifdef __cplusplus
extern "C" /* function pointer member requires an explicit extern "C" declaration to be passed safely from C++ to C */
#endif
struct plum_callback {
  int (* callback) (void * userdata, void * buffer, int size);
  void * userdata;
};

struct plum_metadata {
  int type;
  size_t size;
  void * data;
  struct plum_metadata * next;
};

struct plum_image {
  uint16_t type;
  uint8_t max_palette_index;
  uint8_t color_format;
  uint32_t frames;
  uint32_t height;
  uint32_t width;
  void * allocator;
  struct plum_metadata * metadata;
#if PLUM_ANON_MEMBERS
  union {
#endif
    void * palette;
#if PLUM_ANON_MEMBERS
    uint16_t * palette16;
    uint32_t * palette32;
    uint64_t * palette64;
  };
  union {
#endif
    void * data;
#if PLUM_ANON_MEMBERS
    uint8_t * data8;
    uint16_t * data16;
    uint32_t * data32;
    uint64_t * data64;
  };
#endif
  void * user;
#ifdef __cplusplus
#include "accessors.hpp"
#endif
};
