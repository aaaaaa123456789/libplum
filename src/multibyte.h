#include <stdint.h>

static inline uint16_t read_le16 (const uint16_t * data) {
  return ((uint16_t) *(const unsigned char *) data) | ((uint16_t) 1[(const unsigned char *) data] << 8);
}

static inline uint16_t read_le16_unaligned (const unsigned char * data) {
  return (uint16_t) *data | ((uint16_t) data[1] << 8);
}

static inline uint32_t read_le32_unaligned (const unsigned char * data) {
  return (uint32_t) *data | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static inline uint16_t read_be16_unaligned (const unsigned char * data) {
  return (uint16_t) data[1] | ((uint16_t) *data << 8);
}

static inline uint32_t read_be32 (const uint32_t * data) {
  return (uint32_t) 3[(const unsigned char *) data] | ((uint32_t) 2[(const unsigned char *) data] << 8) |
         ((uint32_t) 1[(const unsigned char *) data] << 16) | ((uint32_t) *(const unsigned char *) data << 24);
}

static inline uint32_t read_be32_unaligned (const unsigned char * data) {
  return (uint32_t) data[3] | ((uint32_t) data[2] << 8) | ((uint32_t) data[1] << 16) | ((uint32_t) *data << 24);
}

static inline void write_le16_unaligned (unsigned char * buffer, uint16_t value) {
  *(buffer ++) = value;
  *(buffer ++) = value >> 8;
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

static inline void write_be16_unaligned (unsigned char * buffer, uint32_t value) {
  *(buffer ++) = value >> 8;
  *buffer = value;
}

static inline void write_be32_unaligned (unsigned char * buffer, uint32_t value) {
  *(buffer ++) = value >> 24;
  *(buffer ++) = value >> 16;
  *(buffer ++) = value >> 8;
  *buffer = value;
}

static inline void write_be32 (uint32_t * buffer, uint32_t value) {
  *((unsigned char *) buffer) = value >> 24;
  1[(unsigned char *) buffer] = value >> 16;
  2[(unsigned char *) buffer] = value >> 8;
  3[(unsigned char *) buffer] = value;
}
