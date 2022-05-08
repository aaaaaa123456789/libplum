#include "proto.h"

uint32_t determine_JPEG_components (struct context * context, size_t offset) {
  uint_fast16_t size = read_be16_unaligned(context -> data + offset);
  if (size < 8) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast8_t count = context -> data[offset + 7];
  if (!count || count > 4) throw(context, PLUM_ERR_INVALID_FILE_FORMAT); // only recognize up to four components
  if (size != 8 + 3 * count) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  unsigned char components[4] = {0};
  for (uint_fast8_t p = 0; p < count; p ++) components[p] = context -> data[offset + 8 + 3 * p];
  #define swap(first, second) do {    \
    uint_fast8_t temp = first;        \
    first = second, second = temp;    \
  } while (false)
  switch (count) {
    // since there's at most four components, a simple swap-based sort is the best implementation
    case 4:
      if (components[3] < *components) swap(*components, components[3]);
      if (components[3] < components[1]) swap(components[1], components[3]);
      if (components[3] < components[2]) swap(components[2], components[3]);
    case 3:
      if (components[2] < *components) swap(*components, components[2]);
      if (components[2] < components[1]) swap(components[1], components[2]);
    case 2:
      if (components[1] < *components) swap(*components, components[1]);
  }
  #undef swap
  for (uint_fast8_t p = 1; p < count; p ++) if (components[p - 1] == components[p]) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  return read_le32_unaligned(components);
}

unsigned get_JPEG_component_count (uint32_t components) {
  if (components < 0x100)
    return 1;
  else if (components < 0x10000u)
    return 2;
  else if (components < 0x1000000u)
    return 3;
  else
    return 4;
}

void (* get_JPEG_component_transfer_function (struct context * context, const struct JPEG_marker_layout * layout, uint32_t components))
      (uint64_t * restrict, size_t, unsigned, const double **) {
  /* The JPEG standard has a very large deficiency: it specifies how to encode an arbitrary set of components of an
     image, but it doesn't specify what those components mean. Components have a single byte ID to identify them, but
     beyond that, the standard just hopes that applications can somehow figure it all out.
     Of course, this means that different extensions make different choices about what components an image can have
     and what those components' IDs should be. Determining the components of an image largely becomes a guessing
     process, typically based on what the IJG's libjpeg does (except that it's not even stable across versions...).
     This function therefore attempts to guess what the image's components mean, and errors out if it can't. */
  if (components < 0x100)
    // if there's only one component, assume the image is just grayscale
    return &JPEG_transfer_grayscale;
  if (layout -> Adobe) {
    if (read_be16_unaligned(context -> data + layout -> Adobe) < 14) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    // Adobe stores a color format ID and specifies four possibilities based on it
    switch (context -> data[layout -> Adobe + 13]) {
      case 0:
        // RGB or CMYK, so check the component count and try to detect the order
        if (components < 0x10000u)
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        else if (components < 0x1000000u)
          if (components == 0x524742u || components == 0x726762u) // 'R', 'G', 'B' (including lowercase)
            return &JPEG_transfer_BGR;
          else if (!((components + 0x102) % 0x10101u)) // any sequential IDs
            return &JPEG_transfer_RGB;
          else
            throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        else
          if (components == 0x594d4b43u || components == 0x796d6b63u) // 'C', 'M', 'Y', 'K' (including lowercase)
            return &JPEG_transfer_CKMY;
          else if (!((components + 0x10203u) % 0x1010101u)) // any sequential IDs
            return &JPEG_transfer_CMYK;
          else
            throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      case 1:
        // YCbCr: verify three components and detect the order
        if (components < 0x10000u || components >= 0x1000000u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (components == 0x635943u) // 'Y', 'C', 'c'
          return &JPEG_transfer_CbYCr;
        else if (!((components + 0x102) % 0x10101u)) // any sequential IDs
          return &JPEG_transfer_YCbCr;
        else
          throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      case 2:
        // YCbCrK: verify four components and detect the order
        if (components < 0x1000000u) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (components == 0x63594b43u) // 'Y', 'C', 'c', 'K'
          return &JPEG_transfer_CbKYCr;
        else if (!((components + 0x10203u) % 0x1010101u)) // any sequential IDs
          return &JPEG_transfer_YCbCrK;
      default:
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
  }
  if (layout -> JFIF) {
    // JFIF mandates one of two possibilities: grayscale (handled already) or YCbCr with IDs of 1, 2, 3 (although some encoders also use 0, 1, 2)
    if (components == 0x30201u || components == 0x20100u) return &JPEG_transfer_YCbCr;
    throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
  // below this line it's pure guesswork: there are no application headers hinting at components, so just guess from popular ID values
  if ((*layout -> frametype & 3) == 3 && components >= 0x10000u && components < 0x1000000u && !((components + 0x102) % 0x10101u))
    // lossless encoding, three sequential component IDs
    return &JPEG_transfer_RGB;
  switch (components) {
    case 0x5941u: // 'Y', 'A'
      return &JPEG_transfer_alpha_grayscale;
    case 0x20100u: // 0, 1, 2: used by libjpeg sometimes
    case 0x30201u: // 1, 2, 3: JFIF's standard IDs
    case 0x232201u: // 1, 0x22, 0x23: used by some library for 'big gamut' colors
      return &JPEG_transfer_YCbCr;
    case 0x635943u: // 'Y', 'C', 'c'
      return &JPEG_transfer_CbYCr;
    case 0x524742u: // 'R', 'G', 'B'
    case 0x726762u: // 'r', 'g', 'b'
      return &JPEG_transfer_BGR;
    case 0x4030201u: // 1, 2, 3, 4
      return &JPEG_transfer_YCbCrK;
    case 0x63594b43u: // 'Y', 'C', 'c', 'K'
      return &JPEG_transfer_CbKYCr;
    case 0x63594341u: // 'Y', 'C', 'c', 'A'
      return &JPEG_transfer_ACbYCr;
    case 0x52474241u: // 'R', 'G', 'B', 'A'
    case 0x72676261u: // 'r', 'g', 'b', 'a'
      return &JPEG_transfer_ABGR;
    case 0x594d4b43u: // 'C', 'M', 'Y', 'K'
    case 0x796d6b63u: // 'c', 'm', 'y', 'k'
      return &JPEG_transfer_CKMY;
    default:
      throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}

void append_JPEG_color_depth_metadata (struct context * context, void (* transfer) (uint64_t * restrict, size_t, unsigned, const double **), unsigned bitdepth) {
  if (transfer == &JPEG_transfer_grayscale)
    add_color_depth_metadata(context, 0, 0, 0, 0, bitdepth);
  else if (transfer == &JPEG_transfer_alpha_grayscale)
    add_color_depth_metadata(context, 0, 0, 0, bitdepth, bitdepth);
  else if (transfer == &JPEG_transfer_ABGR || transfer == &JPEG_transfer_ACbYCr)
    add_color_depth_metadata(context, bitdepth, bitdepth, bitdepth, bitdepth, 0);
  else
    add_color_depth_metadata(context, bitdepth, bitdepth, bitdepth, 0, 0);
}

void JPEG_transfer_RGB (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / limit;
  const double * red = *input;
  const double * green = input[1];
  const double * blue = input[2];
  while (count --) *(output ++) = color_from_floats(*(red ++) * factor, *(green ++) * factor, *(blue ++) * factor, 0);
}

void JPEG_transfer_BGR (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  JPEG_transfer_RGB(output, count, limit, (const double * []) {input[2], input[1], *input});
}

void JPEG_transfer_ABGR (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / limit;
  const double * red = input[3];
  const double * green = input[2];
  const double * blue = input[1];
  const double * alpha = *input;
  while (count --) *(output ++) = color_from_floats(*(red ++) * factor, *(green ++) * factor, *(blue ++) * factor, (limit - *(alpha ++)) * factor);
}

void JPEG_transfer_grayscale (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / limit;
  const double * luma = *input;
  while (count --) {
    double scaled = *(luma ++) * factor;
    *(output ++) = color_from_floats(scaled, scaled, scaled, 0);
  }
}

void JPEG_transfer_alpha_grayscale (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / limit;
  const double * luma = input[1];
  const double * alpha = *input;
  while (count --) {
    double scaled = *(luma ++) * factor;
    *(output ++) = color_from_floats(scaled, scaled, scaled, (limit - *(alpha ++)) * factor);
  }
}

// all constants are defined to have exactly 53 bits of precision (matching IEEE 754 doubles)
#define RED_COEF      0x0.b374bc6a7ef9d8p+0
#define BLUE_COEF     0x0.e2d0e560418938p+0
#define GREEN_CR_COEF 0x0.5b68d15d0f6588p+0
#define GREEN_CB_COEF 0x0.2c0ca8674cd62ep+0

void JPEG_transfer_YCbCr (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / limit;
  const double * luma = *input;
  const double * blue_chroma = input[1];
  const double * red_chroma = input[2];
  while (count --) {
    double blue_offset = limit - *(blue_chroma ++) * 2;
    double red_offset = limit - *(red_chroma ++) * 2;
    double red = *luma - RED_COEF * red_offset, blue = *luma - BLUE_COEF * blue_offset;
    double green = *luma + GREEN_CB_COEF * blue_offset + GREEN_CR_COEF * red_offset;
    luma ++;
    *(output ++) = color_from_floats(red * factor, green * factor, blue * factor, 0);
  }
}

void JPEG_transfer_CbYCr (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  JPEG_transfer_YCbCr(output, count, limit, (const double * []) {input[1], *input, input[2]});
}

void JPEG_transfer_YCbCrK (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  // this function replicates the YCbCr transfer function, but then darkens each color by the K component
  double factor = 65535.0 / ((uint32_t) limit * limit);
  const double * luma = *input;
  const double * blue_chroma = input[1];
  const double * red_chroma = input[2];
  const double * black = input[3];
  while (count --) {
    double blue_offset = limit - *(blue_chroma ++) * 2;
    double red_offset = limit - *(red_chroma ++) * 2;
    double red = *luma - RED_COEF * red_offset, blue = *luma - BLUE_COEF * blue_offset;
    double green = *luma + GREEN_CB_COEF * blue_offset + GREEN_CR_COEF * red_offset;
    luma ++;
    double scale = *(black ++) * factor;
    *(output ++) = color_from_floats(red * scale, green * scale, blue * scale, 0);
  }
}

void JPEG_transfer_CbKYCr (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  JPEG_transfer_YCbCrK(output, count, limit, (const double * []) {input[2], *input, input[3], input[1]});
}

void JPEG_transfer_ACbYCr (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  // this function replicates the YCbCr transfer function and computes a separate alpha channel
  double factor = 65535.0 / limit;
  const double * luma = input[2];
  const double * blue_chroma = input[1];
  const double * red_chroma = input[3];
  const double * alpha = *input;
  while (count --) {
    double blue_offset = limit - *(blue_chroma ++) * 2;
    double red_offset = limit - *(red_chroma ++) * 2;
    double red = *luma - RED_COEF * red_offset, blue = *luma - BLUE_COEF * blue_offset;
    double green = *luma + GREEN_CB_COEF * blue_offset + GREEN_CR_COEF * red_offset;
    luma ++;
    *(output ++) = color_from_floats(red * factor, green * factor, blue * factor, (limit - *(alpha ++)) * factor);
  }
}

#undef RED_COEF
#undef BLUE_COEF
#undef GREEN_CR_COEF
#undef GREEN_CB_COEF

void JPEG_transfer_CMYK (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  double factor = 65535.0 / ((uint32_t) limit * limit);
  const double * cyan = *input;
  const double * magenta = input[1];
  const double * yellow = input[2];
  const double * black = input[3];
  while (count --) {
    double scale = *(black ++) * factor;
    *(output ++) = color_from_floats(*(cyan ++) * scale, *(magenta ++) * scale, *(yellow ++) * scale, 0);
  }
}

void JPEG_transfer_CKMY (uint64_t * restrict output, size_t count, unsigned limit, const double ** input) {
  JPEG_transfer_CMYK(output, count, limit, (const double * []) {*input, input[2], input[3], input[1]});
}
