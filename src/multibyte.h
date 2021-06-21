#include <stdint.h>

static inline uint16_t read_le16 (const uint16_t * data) {
  return ((uint16_t) *(unsigned char *) data) | ((uint16_t) 1[(unsigned char *) data] << 8);
}

static inline uint16_t read_le16_unaligned (const unsigned char * data) {
  return (uint16_t) *data | ((uint16_t) data[1] << 8);
}

static inline uint32_t read_le32_unaligned (const unsigned char * data) {
  return (uint32_t) *data | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static inline void write_le16 (uint16_t * buffer, uint16_t value) {
  *((unsigned char *) buffer) = value;
  1[(unsigned char *) buffer] = value >> 8;
}

static inline void write_le32_unaligned (unsigned char * buffer, uint32_t value) {
  *(buffer ++) = value;
  *(buffer ++) = value >> 8;
  *(buffer ++) = value >> 16;
  *buffer = value >> 24;
}

static inline void write_le32 (uint32_t * buffer, uint32_t value) {
  *((unsigned char *) buffer) = value;
  1[(unsigned char *) buffer] = value >> 8;
  2[(unsigned char *) buffer] = value >> 16;
  3[(unsigned char *) buffer] = value >> 24;
}
