#define PLUM_PIXEL_INDEX(image, col, row, frame) (((size_t) (frame) * (size_t) (image) -> height + (size_t) (row)) * (size_t) (image) -> width + (size_t) (col))

#define PLUM_PIXEL_8(image, col, row, frame) (((uint8_t *) (image) -> data)[PLUM_PIXEL_INDEX(image, col, row, frame)])
#define PLUM_PIXEL_16(image, col, row, frame) (((uint16_t *) (image) -> data)[PLUM_PIXEL_INDEX(image, col, row, frame)])
#define PLUM_PIXEL_32(image, col, row, frame) (((uint32_t *) (image) -> data)[PLUM_PIXEL_INDEX(image, col, row, frame)])
#define PLUM_PIXEL_64(image, col, row, frame) (((uint64_t *) (image) -> data)[PLUM_PIXEL_INDEX(image, col, row, frame)])

#if PLUM_VLA_SUPPORT
#define PLUM_PIXEL_ARRAY_TYPE(image) ((*)[(image) -> height][(image) -> width])
#define PLUM_PIXEL_ARRAY(declarator, image) ((* (declarator))[(image) -> height][(image) -> width])

#define PLUM_PIXELS_8(image) ((uint8_t PLUM_PIXEL_ARRAY_TYPE(image)) (image) -> data)
#define PLUM_PIXELS_16(image) ((uint16_t PLUM_PIXEL_ARRAY_TYPE(image)) (image) -> data)
#define PLUM_PIXELS_32(image) ((uint32_t PLUM_PIXEL_ARRAY_TYPE(image)) (image) -> data)
#define PLUM_PIXELS_64(image) ((uint64_t PLUM_PIXEL_ARRAY_TYPE(image)) (image) -> data)
#endif
