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

void apply_JPEG_inverse_DCT (double output[restrict static 64], int16_t input[restrict static 64], uint16_t quantization[restrict static 64]) {
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
  // row and column of each coefficient, in zigzag order
  static const unsigned char rows[] = {0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3,
                                       4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 6, 7, 7};
  static const unsigned char cols[] = {0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4,
                                       3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 5, 6, 7, 7, 6, 7};
  double dequantized[64];
  uint_fast8_t row, col, index, p = 0;
  for (index = 0; index < 64; index ++) dequantized[index] = (double) input[index] * quantization[index];
  for (row = 0; row < 8; row ++) for (col = 0; col < 8; col ++) {
    output[p] = 0;
    for (index = 0; index < 64; index ++) output[p] += coefficients[col][cols[index]] * coefficients[row][rows[index]] * dequantized[index];
    p ++;
  }
}

#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
