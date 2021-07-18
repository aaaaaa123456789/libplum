#define PLUM_PIXEL_INDEX(image, col, row, frame) (((size_t) frame * (size_t) (image) -> height + (size_t) row) * (size_t) (image) -> width + (size_t) col)

#if PLUM_VLA_SUPPORT
#define PLUM_PIXEL_ARRAY_TYPE(image) ((*)[(image) -> height][(image) -> width])
#define PLUM_PIXEL_ARRAY(declarator, image) ((* (declarator))[(image) -> height][(image) -> width])
#endif

#define PLUM_COLOR_32(red, green, blue, alpha) ((uint32_t) (((uint32_t) (red) & 0xff) | (((uint32_t) (green) & 0xff) << 8) | \
                                                            (((uint32_t) (blue) & 0xff) << 16) | (((uint32_t) (alpha) & 0xff) << 24)))
#define PLUM_COLOR_64(red, green, blue, alpha) ((uint64_t) (((uint64_t) (red) & 0xffffu) | (((uint64_t) (green) & 0xffffu) << 16) | \
                                                            (((uint64_t) (blue) & 0xffffu) << 32) | (((uint64_t) (alpha) & 0xffffu) << 48)))
#define PLUM_COLOR_16(red, green, blue, alpha) ((uint16_t) (((uint16_t) (red) & 0x1f) | (((uint16_t) (green) & 0x1f) << 5) | \
                                                            (((uint16_t) (blue) & 0x1f) << 10) | (((uint16_t) (alpha) & 1) << 15)))
#define PLUM_COLOR_32X(red, green, blue, alpha) ((uint32_t) (((uint32_t) (red) & 0x3ff) | (((uint32_t) (green) & 0x3ff) << 10) | \
                                                             (((uint32_t) (blue) & 0x3ff) << 20) | (((uint32_t) (alpha) & 3) << 30)))

#define PLUM_RED_32(color) ((uint32_t) ((uint32_t) (color) & 0xff))
#define PLUM_RED_64(color) ((uint64_t) ((uint64_t) (color) & 0xffffu))
#define PLUM_RED_16(color) ((uint16_t) ((uint16_t) (color) & 0x1f))
#define PLUM_RED_32X(color) ((uint32_t) ((uint32_t) (color) & 0x3ff))
#define PLUM_GREEN_32(color) ((uint32_t) (((uint32_t) (color) >> 8) & 0xff))
#define PLUM_GREEN_64(color) ((uint64_t) (((uint64_t) (color) >> 16) & 0xffffu))
#define PLUM_GREEN_16(color) ((uint16_t) (((uint16_t) (color) >> 5) & 0x1f))
#define PLUM_GREEN_32X(color) ((uint32_t) (((uint32_t) (color) >> 10) & 0x3ff))
#define PLUM_BLUE_32(color) ((uint32_t) (((uint32_t) (color) >> 16) & 0xff))
#define PLUM_BLUE_64(color) ((uint64_t) (((uint64_t) (color) >> 32) & 0xffffu))
#define PLUM_BLUE_16(color) ((uint16_t) (((uint16_t) (color) >> 10) & 0x1f))
#define PLUM_BLUE_32X(color) ((uint32_t) (((uint32_t) (color) >> 20) & 0x3ff))
#define PLUM_ALPHA_32(color) ((uint32_t) (((uint32_t) (color) >> 24) & 0xff))
#define PLUM_ALPHA_64(color) ((uint64_t) (((uint64_t) (color) >> 48) & 0xffffu))
#define PLUM_ALPHA_16(color) ((uint16_t) (((uint16_t) (color) >> 15) & 1))
#define PLUM_ALPHA_32X(color) ((uint32_t) (((uint32_t) (color) >> 30) & 3))

#define PLUM_RED_MASK_32 ((uint32_t) 0xff)
#define PLUM_RED_MASK_64 ((uint64_t) 0xffffu)
#define PLUM_RED_MASK_16 ((uint16_t) 0x1f)
#define PLUM_RED_MASK_32X ((uint32_t) 0x3ff)
#define PLUM_GREEN_MASK_32 ((uint32_t) 0xff00u)
#define PLUM_GREEN_MASK_64 ((uint64_t) 0xffff0000u)
#define PLUM_GREEN_MASK_16 ((uint16_t) 0x3e0)
#define PLUM_GREEN_MASK_32X ((uint32_t) 0xffc00u)
#define PLUM_BLUE_MASK_32 ((uint32_t) 0xff0000u)
#define PLUM_BLUE_MASK_64 ((uint64_t) 0xffff00000000u)
#define PLUM_BLUE_MASK_16 ((uint16_t) 0x7c00)
#define PLUM_BLUE_MASK_32X ((uint32_t) 0x3ff00000u)
#define PLUM_ALPHA_MASK_32 ((uint32_t) 0xff000000u)
#define PLUM_ALPHA_MASK_64 ((uint64_t) 0xffff000000000000u)
#define PLUM_ALPHA_MASK_16 ((uint16_t) 0x8000u)
#define PLUM_ALPHA_MASK_32X ((uint32_t) 0xc0000000u)
