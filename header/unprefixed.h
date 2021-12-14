#define PIXEL(image, col, row, frame) PLUM_PIXEL_INDEX(image, col, row, frame)

#define PIXEL8(image, col, row, frame) PLUM_PIXEL_8(image, col, row, frame)
#define PIXEL16(image, col, row, frame) PLUM_PIXEL_16(image, col, row, frame)
#define PIXEL32(image, col, row, frame) PLUM_PIXEL_32(image, col, row, frame)
#define PIXEL64(image, col, row, frame) PLUM_PIXEL_64(image, col, row, frame)

#if PLUM_VLA_SUPPORT
#define PIXARRAY_T(image) PLUM_PIXEL_ARRAY_TYPE(image)
#define PIXARRAY(declarator, image) PLUM_PIXEL_ARRAY(declarator, image)

#define PIXELS8(image) PLUM_PIXELS_8(image)
#define PIXELS16(image) PLUM_PIXELS_16(image)
#define PIXELS32(image) PLUM_PIXELS_32(image)
#define PIXELS64(image) PLUM_PIXELS_64(image)
#endif

#define COLOR32(red, green, blue, alpha) PLUM_COLOR_VALUE_32(red, green, blue, alpha)
#define COLOR64(red, green, blue, alpha) PLUM_COLOR_VALUE_64(red, green, blue, alpha)
#define COLOR16(red, green, blue, alpha) PLUM_COLOR_VALUE_16(red, green, blue, alpha)
#define COLOR32X(red, green, blue, alpha) PLUM_COLOR_VALUE_32X(red, green, blue, alpha)

#define RED32(color) PLUM_RED_32(color)
#define RED64(color) PLUM_RED_64(color)
#define RED16(color) PLUM_RED_16(color)
#define RED32X(color) PLUM_RED_32X(color)
#define GREEN32(color) PLUM_GREEN_32(color)
#define GREEN64(color) PLUM_GREEN_64(color)
#define GREEN16(color) PLUM_GREEN_16(color)
#define GREEN32X(color) PLUM_GREEN_32X(color)
#define BLUE32(color) PLUM_BLUE_32(color)
#define BLUE64(color) PLUM_BLUE_64(color)
#define BLUE16(color) PLUM_BLUE_16(color)
#define BLUE32X(color) PLUM_BLUE_32X(color)
#define ALPHA32(color) PLUM_ALPHA_32(color)
#define ALPHA64(color) PLUM_ALPHA_64(color)
#define ALPHA16(color) PLUM_ALPHA_16(color)
#define ALPHA32X(color) PLUM_ALPHA_32X(color)
