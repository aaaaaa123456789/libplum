inline uint8_t & pixel8 (uint32_t col, uint32_t row, uint32_t frame = 0) {
  return ((uint8_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline const uint8_t & pixel8 (uint32_t col, uint32_t row, uint32_t frame = 0) const {
  return ((const uint8_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline uint16_t & pixel16 (uint32_t col, uint32_t row, uint32_t frame = 0) {
  return ((uint16_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline const uint16_t & pixel16 (uint32_t col, uint32_t row, uint32_t frame = 0) const {
  return ((const uint16_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline uint32_t & pixel32 (uint32_t col, uint32_t row, uint32_t frame = 0) {
  return ((uint32_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline const uint32_t & pixel32 (uint32_t col, uint32_t row, uint32_t frame = 0) const {
  return ((const uint32_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline uint64_t & pixel64 (uint32_t col, uint32_t row, uint32_t frame = 0) {
  return ((uint64_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline const uint64_t & pixel64 (uint32_t col, uint32_t row, uint32_t frame = 0) const {
  return ((const uint64_t *) this -> data)[PLUM_PIXEL_INDEX(this, col, row, frame)];
}

inline uint16_t & color16 (uint8_t index) {
  return ((uint16_t *) this -> palette)[index];
}

inline const uint16_t & color16 (uint8_t index) const {
  return ((const uint16_t *) this -> palette)[index];
}

inline uint32_t & color32 (uint8_t index) {
  return ((uint32_t *) this -> palette)[index];
}

inline const uint32_t & color32 (uint8_t index) const {
  return ((const uint32_t *) this -> palette)[index];
}

inline uint64_t & color64 (uint8_t index) {
  return ((uint64_t *) this -> palette)[index];
}

inline const uint64_t & color64 (uint8_t index) const {
  return ((const uint64_t *) this -> palette)[index];
}
