#include <stddef.h>
#include <setjmp.h>

#include "../header/libplum.h"

typedef void JPEG_component_transfer_function(uint64_t * restrict, size_t, unsigned, const double **);

union allocator_node {
  max_align_t alignment;
  struct {
    union allocator_node * previous;
    union allocator_node * next;
  };
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
  union allocator_node * allocator;
  union {
    struct plum_image * image;
    const struct plum_image * source;
  };
  jmp_buf target;
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
  uint32_t index:   8;
  uint32_t tableQ:  8;
  uint32_t tableDC: 4;
  uint32_t tableAC: 4;
  uint32_t scaleH:  4;
  uint32_t scaleV:  4;
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
  unsigned switch_MPS:   1;
  unsigned next_MPS:     8;
  unsigned next_LPS:     8;
};

struct JPEG_encoded_value {
  unsigned code:   8;
  unsigned type:   1; // 0 for DC codes, 1 for AC codes
  unsigned bits:   7;
  unsigned value: 16;
};
