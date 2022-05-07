#include "proto.h"

// Cx = 0.5 * cos(x * pi / 16), rounded so that it fits exactly in 53 bits (standard precision for IEEE doubles)
// note that C4 = 0.5 / sqrt(2), so this value is also used for that purpose
#define C1 0x0.7d8a5f3fdd72c0p+0
#define C2 0x0.7641af3cca3518p+0
#define C3 0x0.6a6d98a43a868cp+0
#define C4 0x0.5a827999fcef34p+0
#define C5 0x0.471cece6b9a320p+0
#define C6 0x0.30fbc54d5d52c6p+0
#define C7 0x0.18f8b83c69a60bp+0

// half the square root of 2
#define HR2 0x0.b504f333f9de68p+0

double apply_JPEG_DCT (int16_t output[restrict static 64], const double input[restrict static 64], const uint8_t quantization[restrict static 64], double prevDC) {
  // coefficient(dst, src) = cos((2 * src + 1) * dst * pi / 16) / 2; this absorbs a leading factor of 1/4 (square rooted)
  static const double coefficients[8][8] = {
    {0.5,  C1,  C2,  C3,  C4,  C5,  C6,  C7},
    {0.5,  C3,  C6, -C7, -C4, -C1, -C2, -C5},
    {0.5,  C5, -C6, -C1, -C4,  C7,  C2,  C3},
    {0.5,  C7, -C2, -C5,  C4,  C3, -C6, -C1},
    {0.5, -C7, -C2,  C5,  C4, -C3, -C6,  C1},
    {0.5, -C5, -C6,  C1, -C4, -C7,  C2, -C3},
    {0.5, -C3,  C6,  C7, -C4,  C1, -C2,  C5},
    {0.5, -C1,  C2, -C3,  C4, -C5,  C6, -C7}
  };
  // factor(row, col) = (row ? 1 : 1 / sqrt(2)) * (col ? 1 : 1 / sqrt(2)); converted into zigzag order
  static const double factors[] = {
    0.5, HR2, HR2, HR2, 1.0, HR2, HR2, 1.0, 1.0, HR2, HR2, 1.0, 1.0, 1.0, HR2, HR2, 1.0, 1.0, 1.0, 1.0, HR2, HR2, 1.0, 1.0, 1.0, 1.0, 1.0, HR2, HR2, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, HR2, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
  };
  // zero-flushing threshold: for later coefficients, round some values slightly larger than 0.5 to 0 instead of +/- 1 for better compression
  static const double zeroflush[] = {
    0x0.80p+0, 0x0.80p+0, 0x0.80p+0, 0x0.80p+0, 0x0.81p+0, 0x0.80p+0, 0x0.84p+0, 0x0.85p+0, 0x0.85p+0, 0x0.84p+0,
    0x0.88p+0, 0x0.89p+0, 0x0.8ap+0, 0x0.89p+0, 0x0.88p+0, 0x0.8cp+0, 0x0.8dp+0, 0x0.8ep+0, 0x0.8ep+0, 0x0.8dp+0,
    0x0.8cp+0, 0x0.90p+0, 0x0.91p+0, 0x0.92p+0, 0x0.93p+0, 0x0.92p+0, 0x0.91p+0, 0x0.90p+0, 0x0.94p+0, 0x0.95p+0,
    0x0.96p+0, 0x0.97p+0, 0x0.97p+0, 0x0.96p+0, 0x0.95p+0, 0x0.94p+0, 0x0.98p+0, 0x0.99p+0, 0x0.9ap+0, 0x0.9bp+0,
    0x0.9ap+0, 0x0.99p+0, 0x0.98p+0, 0x0.9cp+0, 0x0.9dp+0, 0x0.9ep+0, 0x0.9ep+0, 0x0.9dp+0, 0x0.9cp+0, 0x0.a0p+0,
    0x0.a1p+0, 0x0.a2p+0, 0x0.a1p+0, 0x0.a0p+0, 0x0.a4p+0, 0x0.a5p+0, 0x0.a5p+0, 0x0.a4p+0, 0x0.a8p+0, 0x0.a9p+0,
    0x0.a8p+0, 0x0.acp+0, 0x0.acp+0, 0x0.b0p+0
  };
  for (uint_fast8_t index = 0; index < 64; index ++) {
    uint_fast8_t p = 0;
    double converted = 0.0;
    for (uint_fast8_t row = 0; row < 8; row ++) for (uint_fast8_t col = 0; col < 8; col ++)
      converted += input[p ++] * coefficients[col][JPEG_zigzag_columns[index]] * coefficients[row][JPEG_zigzag_rows[index]];
    converted = converted * factors[index] / quantization[index];
    if (index)
      if (converted >= -zeroflush[index] && converted <= zeroflush[index])
        output[index] = 0;
      else if (converted > 1023.0)
        output[index] = 1023;
      else if (converted < -1023.0)
        output[index] = -1023;
      else if (converted < 0)
        output[index] = converted - 0.5;
      else
        output[index] = converted + 0.5;
    else {
      converted -= prevDC;
      if (converted > 2047.0)
        *output = 2047;
      else if (converted < -2047.0)
        *output = -2047;
      else if (converted < 0)
        *output = converted - 0.5;
      else
        *output = converted + 0.5;
    }
  }
  return prevDC + *output;
}

void apply_JPEG_inverse_DCT (double output[restrict static 64], const int16_t input[restrict static 64], const uint16_t quantization[restrict static 64]) {
  // coefficient(dst, src) = 0.5 * (src ? cos((2 * dst + 1) * src * pi / 16) : 1 / sqrt(2)); this absorbs a leading factor of 1/4 (square rooted)
  static const double coefficients[8][8] = {
    {C4,  C1,  C2,  C3,  C4,  C5,  C6,  C7},
    {C4,  C3,  C6, -C7, -C4, -C1, -C2, -C5},
    {C4,  C5, -C6, -C1, -C4,  C7,  C2,  C3},
    {C4,  C7, -C2, -C5,  C4,  C3, -C6, -C1},
    {C4, -C7, -C2,  C5,  C4, -C3, -C6,  C1},
    {C4, -C5, -C6,  C1, -C4, -C7,  C2, -C3},
    {C4, -C3,  C6,  C7, -C4,  C1, -C2,  C5},
    {C4, -C1,  C2, -C3,  C4, -C5,  C6, -C7}
  };
  double dequantized[64];
  for (uint_fast8_t index = 0; index < 64; index ++) dequantized[index] = (double) input[index] * quantization[index];
  uint_fast8_t p = 0;
  for (uint_fast8_t row = 0; row < 8; row ++) for (uint_fast8_t col = 0; col < 8; col ++) {
    output[p] = 0;
    for (uint_fast8_t index = 0; index < 64; index ++)
      output[p] += coefficients[col][JPEG_zigzag_columns[index]] * coefficients[row][JPEG_zigzag_rows[index]] * dequantized[index];
    p ++;
  }
}

#undef HR2
#undef C7
#undef C6
#undef C5
#undef C4
#undef C3
#undef C2
#undef C1
