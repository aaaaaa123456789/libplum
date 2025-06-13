#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <setjmp.h>

#include "../header/libplum.h"

struct allocator_node {
  struct allocator_node * previous;
  struct allocator_node * next;
  alignas(max_align_t) unsigned char data[];
};

struct data_node {
  union {
    struct {
      size_t size;
      struct data_node * previous;
      struct data_node * next;
    };
    max_align_t alignment;
  };
  unsigned char data[];
};

struct context {
  unsigned status;
  size_t size;
  union {
    const unsigned char * data;
    struct data_node * output; // reverse order: top of the list is the LAST node
  };
  struct allocator_node * allocator;
  union {
    struct plum_image * image;
    const struct plum_image * source;
  };
  FILE * file;
  jmp_buf target;
};

struct pair {
  size_t value;
  size_t index;
};

struct compressed_GIF_code {
  alignas(uint32_t) int16_t reference; // align the first member to align the struct
  unsigned char value;
  unsigned char type;
};

struct PNG_chunk_locations {
  // includes APNG chunks; IHDR and IEND omitted because IHDR has a fixed offset and IEND contains no data
  size_t palette; // PLTE
  size_t bits; // sBIT
  size_t background; // bKGD
  size_t transparency; // tRNS
  size_t animation; // acTL
  size_t * data; // IDAT
  size_t * frameinfo; // fcTL
  size_t ** framedata; // fdAT
};

struct compressed_PNG_code {
  unsigned datacode:   9;
  unsigned dataextra:  5;
  unsigned distcode:   5;
  unsigned distextra: 13;
};

struct JPEG_marker_layout {
  unsigned char * frametype; // 0-15
  size_t * frames;
  size_t ** framescans;
  size_t *** framedata; // for each frame, for each scan, for each restart interval: offset, size
  unsigned char * markertype; // same as the follow-up byte from the marker itself
  size_t * markers; // for some markers only (DHT, DAC, DQT, DNL, DRI, EXP)
  size_t hierarchical; // DHP marker, if present
  size_t JFIF;
  size_t Exif;
  size_t Adobe;
};

struct JPEG_decoder_tables {
  short * Huffman[8]; // 4 DC, 4 AC
  unsigned short * quantization[4];
  unsigned char arithmetic[8]; // conditioning values: 4 DC, 4 AC
  uint16_t restart;
};

struct JPEG_component_info {
  unsigned index:   8;
  unsigned tableQ:  8;
  unsigned tableDC: 4;
  unsigned tableAC: 4;
  unsigned scaleH:  4;
  unsigned scaleV:  4;
};

struct JPEG_decompressor_state {
  union {
    int16_t (* restrict current_block[4])[64];
    uint16_t * restrict current_value[4];
  };
  size_t last_size;
  size_t restart_count;
  uint16_t row_skip_index;
  uint16_t row_skip_count;
  uint16_t column_skip_index;
  uint16_t column_skip_count;
  uint16_t row_offset[4];
  uint16_t unit_row_offset[4];
  uint8_t unit_offset[4];
  uint16_t restart_size;
  unsigned char component_count;
  unsigned char MCU[81];
};

enum JPEG_MCU_control_codes {
  MCU_ZERO_COORD = 0xfd,
  MCU_NEXT_ROW   = 0xfe,
  MCU_END_LIST   = 0xff
};

struct JPEG_arithmetic_decoder_state {
  unsigned probability: 15;
  bool switch_MPS:       1;
  unsigned next_MPS:     8;
  unsigned next_LPS:     8;
};

struct JPEG_encoded_value {
  unsigned code:   8;
  unsigned type:   1; // 0 for DC codes, 1 for AC codes
  unsigned bits:   7;
  unsigned value: 16;
};

struct PNM_image_header {
  uint8_t type; // 1-6: PNM header types, 7: unknown PAM, 11-13: PAM without alpha (B/W, grayscale, RGB), 14-16: PAM with alpha
  uint16_t maxvalue;
  uint32_t width;
  uint32_t height;
  size_t datastart;
  size_t datalength;
};

struct QOI_pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};
