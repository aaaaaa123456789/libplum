#include <stdint.h>

static inline uint16_t read_le16_unaligned (const unsigned char * data) {
  return (uint16_t) *data | ((uint16_t) data[1] << 8);
}

static inline uint32_t read_le32_unaligned (const unsigned char * data) {
  return (uint32_t) *data | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static inline uint16_t read_be16_unaligned (const unsigned char * data) {
  return (uint16_t) data[1] | ((uint16_t) *data << 8);
}

static inline uint32_t read_be32_unaligned (const unsigned char * data) {
  return (uint32_t) data[3] | ((uint32_t) data[2] << 8) | ((uint32_t) data[1] << 16) | ((uint32_t) *data << 24);
}

static inline void write_le16_unaligned (unsigned char * buffer, uint16_t value) {
  bytewrite(buffer, value, value >> 8);
}

static inline void write_le32_unaligned (unsigned char * buffer, uint32_t value) {
  bytewrite(buffer, value, value >> 8, value >> 16, value >> 24);
}

static inline void write_be16_unaligned (unsigned char * buffer, uint32_t value) {
  bytewrite(buffer, value >> 8, value);
}

static inline void write_be32_unaligned (unsigned char * buffer, uint32_t value) {
  bytewrite(buffer, value >> 24, value >> 16, value >> 8, value);
}
