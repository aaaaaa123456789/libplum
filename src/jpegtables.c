#include "proto.h"

void initialize_JPEG_decoder_tables (struct context * context, struct JPEG_decoder_tables * tables, const struct JPEG_marker_layout * layout) {
  *tables = (struct JPEG_decoder_tables) {
    .Huffman = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    .quantization = {NULL, NULL, NULL, NULL},
    .arithmetic = {0x10, 0x10, 0x10, 0x10, 5, 5, 5, 5},
    .restart = 0
  };
  // if the image doesn't define a Huffman table (no DHT markers), load the standard's recommended tables as "default" tables
  for (size_t p = 0; layout -> markers[p]; p ++) if (layout -> markertype[p] == 0xc4) return;
  load_default_JPEG_Huffman_tables(context, tables);
}

short * process_JPEG_Huffman_table (struct context * context, const unsigned char ** restrict markerdata, uint16_t * restrict markersize) {
  if (*markersize < 16) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast16_t totalsize = 0, tablesize = 16; // 16 so it counts the initial length bytes too
  const unsigned char * lengths = *markerdata;
  const unsigned char * data = *markerdata + 16;
  for (uint_fast8_t size = 0; size < 16; size ++) {
    tablesize += lengths[size];
    totalsize += lengths[size] * (size + 1) * 2; // not necessarily the real size of the table, but an easily calculated upper bound
  }
  if (*markersize < tablesize) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  *markersize -= tablesize;
  *markerdata += tablesize;
  short * result = ctxmalloc(context, totalsize * sizeof *result);
  for (uint_fast16_t p = 0; p < totalsize; p ++) result[p] = -1;
  uint_fast16_t code = 0, next = 2, offset = 0x8000u;
  // size is one less because we don't count the link to the leaf
  for (uint_fast8_t size = 0; offset; size ++, offset >>= 1) for (uint_fast8_t count = lengths[size]; count; count --) {
    uint_fast16_t current = 0x8000u, index = 0;
    for (uint_fast8_t remainder = size; remainder; remainder --) {
      if (code & current) index ++;
      current >>= 1;
      if (result[index] == -1) {
        result[index] = -(short) next;
        next += 2;
      }
      index = -result[index];
    }
    if (code & current) index ++;
    result[index] = *(data ++);
    if ((uint_fast32_t) code + offset > 0xffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    code += offset;
  }
  return ctxrealloc(context, result, next * sizeof *result);
}

void load_default_JPEG_Huffman_tables (struct context * context, struct JPEG_decoder_tables * tables) {
  /* default tables from the JPEG specification, already preprocessed into a tree, in the same format as other trees:
     two values per node, non-negative values are leaves, negative values are array indexes where the next node is
     found (always even, because each node takes up two entries), -1 is an empty branch; index 0 is the root node */
  static const short luminance_DC_table[] = {
    //           0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19
    /*   0 */   -2,   -6, 0x00,   -4, 0x01, 0x02,   -8,  -10, 0x03, 0x04, 0x05,  -12, 0x06,  -14, 0x07,  -16, 0x08,  -18, 0x09,  -20,
    /*  20 */ 0x0a,  -22, 0x0b,   -1
  };
  static const short chrominance_DC_table[] = {
    //           0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19
    /*   0 */   -2,   -4, 0x00, 0x01, 0x02,   -6, 0x03,   -8, 0x04,  -10, 0x05,  -12, 0x06,  -14, 0x07,  -16, 0x08,  -18, 0x09,  -20,
    /*  20 */ 0x0a,  -22, 0x0b,   -1
  };
  static const short luminance_AC_table[] = {
    //           0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19
    /*   0 */   -2,   -4, 0x01, 0x02,   -6,  -10, 0x03,   -8, 0x00, 0x04,  -12,  -16, 0x11,  -14, 0x05, 0x12,  -18,  -22, 0x21,  -20,
    /*  20 */ 0x31, 0x41,  -24,  -30,  -26,  -28, 0x06, 0x13, 0x51, 0x61,  -32,  -40,  -34,  -36, 0x07, 0x22, 0x71,  -38, 0x14, 0x32,
    /*  40 */  -42,  -50,  -44,  -46, 0x81, 0x91, 0xa1,  -48, 0x08, 0x23,  -52,  -60,  -54,  -56, 0x42, 0xb1, 0xc1,  -58, 0x15, 0x52,
    /*  60 */  -62,  -72,  -64,  -66, 0xd1, 0xf0,  -68,  -70, 0x24, 0x33, 0x62, 0x72,  -74, -198,  -76, -136,  -78, -106,  -80,  -92,
    /*  80 */  -82,  -86, 0x82,  -84, 0x09, 0x0a,  -88,  -90, 0x16, 0x17, 0x18, 0x19,  -94, -100,  -96,  -98, 0x1a, 0x25, 0x26, 0x27,
    /* 100 */ -102, -104, 0x28, 0x29, 0x2a, 0x34, -108, -122, -110, -116, -112, -114, 0x35, 0x36, 0x37, 0x38, -118, -120, 0x39, 0x3a,
    /* 120 */ 0x43, 0x44, -124, -130, -126, -128, 0x45, 0x46, 0x47, 0x48, -132, -134, 0x49, 0x4a, 0x53, 0x54, -138, -168, -140, -154,
    /* 140 */ -142, -148, -144, -146, 0x55, 0x56, 0x57, 0x58, -150, -152, 0x59, 0x5a, 0x63, 0x64, -156, -162, -158, -160, 0x65, 0x66,
    /* 160 */ 0x67, 0x68, -164, -166, 0x69, 0x6a, 0x73, 0x74, -170, -184, -172, -178, -174, -176, 0x75, 0x76, 0x77, 0x78, -180, -182,
    /* 180 */ 0x79, 0x7a, 0x83, 0x84, -186, -192, -188, -190, 0x85, 0x86, 0x87, 0x88, -194, -196, 0x89, 0x8a, 0x92, 0x93, -200, -262,
    /* 200 */ -202, -232, -204, -218, -206, -212, -208, -210, 0x94, 0x95, 0x96, 0x97, -214, -216, 0x98, 0x99, 0x9a, 0xa2, -220, -226,
    /* 220 */ -222, -224, 0xa3, 0xa4, 0xa5, 0xa6, -228, -230, 0xa7, 0xa8, 0xa9, 0xaa, -234, -248, -236, -242, -238, -240, 0xb2, 0xb3,
    /* 240 */ 0xb4, 0xb5, -244, -246, 0xb6, 0xb7, 0xb8, 0xb9, -250, -256, -252, -254, 0xba, 0xc2, 0xc3, 0xc4, -258, -260, 0xc5, 0xc6,
    /* 260 */ 0xc7, 0xc8, -264, -294, -266, -280, -268, -274, -270, -272, 0xc9, 0xca, 0xd2, 0xd3, -276, -278, 0xd4, 0xd5, 0xd6, 0xd7,
    /* 280 */ -282, -288, -284, -286, 0xd8, 0xd9, 0xda, 0xe1, -290, -292, 0xe2, 0xe3, 0xe4, 0xe5, -296, -310, -298, -304, -300, -302,
    /* 300 */ 0xe6, 0xe7, 0xe8, 0xe9, -306, -308, 0xea, 0xf1, 0xf2, 0xf3, -312, -318, -314, -316, 0xf4, 0xf5, 0xf6, 0xf7, -320, -322,
    /* 320 */ 0xf8, 0xf9, 0xfa,   -1
  };
  static const short chrominance_AC_table[] = {
    //           0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19
    /*   0 */   -2,   -4, 0x00, 0x01,   -6,  -10, 0x02,   -8, 0x03, 0x11,  -12,  -18,  -14,  -16, 0x04, 0x05, 0x21, 0x31,  -20,  -26,
    /*  20 */  -22,  -24, 0x06, 0x12, 0x41, 0x51,  -28,  -36,  -30,  -32, 0x07, 0x61, 0x71,  -34, 0x13, 0x22,  -38,  -48,  -40,  -42,
    /*  40 */ 0x32, 0x81,  -44,  -46, 0x08, 0x14, 0x42, 0x91,  -50,  -58,  -52,  -54, 0xa1, 0xb1, 0xc1,  -56, 0x09, 0x23,  -60,  -68,
    /*  60 */  -62,  -64, 0x33, 0x52, 0xf0,  -66, 0x15, 0x62,  -70,  -80,  -72,  -74, 0x72, 0xd1,  -76,  -78, 0x0a, 0x16, 0x24, 0x34,
    /*  80 */  -82, -198,  -84, -136,  -86, -106,  -88,  -92, 0xe1,  -90, 0x25, 0xf1,  -94, -100,  -96,  -98, 0x17, 0x18, 0x19, 0x1a,
    /* 100 */ -102, -104, 0x26, 0x27, 0x28, 0x29, -108, -122, -110, -116, -112, -114, 0x2a, 0x35, 0x36, 0x37, -118, -120, 0x38, 0x39,
    /* 120 */ 0x3a, 0x43, -124, -130, -126, -128, 0x44, 0x45, 0x46, 0x47, -132, -134, 0x48, 0x49, 0x4a, 0x53, -138, -168, -140, -154,
    /* 140 */ -142, -148, -144, -146, 0x54, 0x55, 0x56, 0x57, -150, -152, 0x58, 0x59, 0x5a, 0x63, -156, -162, -158, -160, 0x64, 0x65,
    /* 160 */ 0x66, 0x67, -164, -166, 0x68, 0x69, 0x6a, 0x73, -170, -184, -172, -178, -174, -176, 0x74, 0x75, 0x76, 0x77, -180, -182,
    /* 180 */ 0x78, 0x79, 0x7a, 0x82, -186, -192, -188, -190, 0x83, 0x84, 0x85, 0x86, -194, -196, 0x87, 0x88, 0x89, 0x8a, -200, -262,
    /* 200 */ -202, -232, -204, -218, -206, -212, -208, -210, 0x92, 0x93, 0x94, 0x95, -214, -216, 0x96, 0x97, 0x98, 0x99, -220, -226,
    /* 220 */ -222, -224, 0x9a, 0xa2, 0xa3, 0xa4, -228, -230, 0xa5, 0xa6, 0xa7, 0xa8, -234, -248, -236, -242, -238, -240, 0xa9, 0xaa,
    /* 240 */ 0xb2, 0xb3, -244, -246, 0xb4, 0xb5, 0xb6, 0xb7, -250, -256, -252, -254, 0xb8, 0xb9, 0xba, 0xc2, -258, -260, 0xc3, 0xc4,
    /* 260 */ 0xc5, 0xc6, -264, -294, -266, -280, -268, -274, -270, -272, 0xc7, 0xc8, 0xc9, 0xca, -276, -278, 0xd2, 0xd3, 0xd4, 0xd5,
    /* 280 */ -282, -288, -284, -286, 0xd6, 0xd7, 0xd8, 0xd9, -290, -292, 0xda, 0xe2, 0xe3, 0xe4, -296, -310, -298, -304, -300, -302,
    /* 300 */ 0xe5, 0xe6, 0xe7, 0xe8, -306, -308, 0xe9, 0xea, 0xf2, 0xf3, -312, -318, -314, -316, 0xf4, 0xf5, 0xf6, 0xf7, -320, -322,
    /* 320 */ 0xf8, 0xf9, 0xfa,   -1
  };
  *tables -> Huffman = memcpy(ctxmalloc(context, sizeof luminance_DC_table), luminance_DC_table, sizeof luminance_DC_table);
  tables -> Huffman[1] = memcpy(ctxmalloc(context, sizeof chrominance_DC_table), chrominance_DC_table, sizeof chrominance_DC_table);
  tables -> Huffman[4] = memcpy(ctxmalloc(context, sizeof luminance_AC_table), luminance_AC_table, sizeof luminance_AC_table);
  tables -> Huffman[5] = memcpy(ctxmalloc(context, sizeof chrominance_AC_table), chrominance_AC_table, sizeof chrominance_AC_table);
}
