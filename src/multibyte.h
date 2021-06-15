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
