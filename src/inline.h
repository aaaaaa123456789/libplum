static inline noreturn throw (struct context * context, unsigned error) {
  context -> status = error;
  longjmp(context -> target, 1);
}

static inline struct allocator_node * get_allocator_node (void * buffer) {
  return (struct allocator_node *) ((char *) buffer - offsetof(struct allocator_node, data));
}

static inline void * ctxmalloc (struct context * context, size_t size) {
  void * result = allocate(&(context -> allocator), size);
  if (!result) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  return result;
}

static inline void * ctxcalloc (struct context * context, size_t size) {
  void * result = clear_allocate(&(context -> allocator), size);
  if (!result) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  return result;
}

static inline void * ctxrealloc (struct context * context, void * buffer, size_t size) {
  void * result = reallocate(&(context -> allocator), buffer, size);
  if (!result) throw(context, PLUM_ERR_OUT_OF_MEMORY);
  return result;
}

static inline void ctxfree (struct context * context, void * buffer) {
  deallocate(&(context -> allocator), buffer);
}

static inline uintmax_t bitnegate (uintmax_t value) {
  // ensure that the value is negated correctly, without accidental unsigned-to-signed conversions getting in the way
  return ~value;
}

static inline uint16_t bitextend16 (uint16_t value, unsigned width) {
  uint_fast32_t result = value;
  while (width < 16) {
    result |= result << width;
    width <<= 1;
  }
  return result >> (width - 16);
}

static inline void * append_output_node (struct context * context, size_t size) {
  struct data_node * node = ctxmalloc(context, sizeof *node + size);
  *node = (struct data_node) {.size = size, .previous = context -> output, .next = NULL};
  if (context -> output) context -> output -> next = node;
  context -> output = node;
  return node -> data;
}

static inline bool bit_depth_less_than (uint32_t depth, uint32_t target) {
  // formally "less than or equal to", but that would be a very long name
  return !((target - depth) & 0x80808080u);
}

static inline int absolute_value (int value) {
  return (value < 0) ? -value : value;
}

static inline uint32_t shift_in_left (struct context * context, unsigned count, uint32_t * restrict dataword, uint8_t * restrict bits,
                                      const unsigned char ** data, size_t * restrict size) {
  while (*bits < count) {
    if (!*size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    *dataword |= (uint32_t) **data << *bits;
    ++ *data;
    -- *size;
    *bits += 8;
  }
  uint32_t result;
  if (count < 32) {
    result = *dataword & (((uint32_t) 1 << count) - 1);
    *dataword >>= count;
  } else {
    result = *dataword;
    *dataword = 0;
  }
  *bits -= count;
  return result;
}

static inline uint32_t shift_in_right_JPEG (struct context * context, unsigned count, uint32_t * restrict dataword, uint8_t * restrict bits,
                                            const unsigned char ** data, size_t * restrict size) {
  // unlike shift_in_left above, this function has to account for stuffed bytes (any number of 0xFF followed by a single 0x00)
  while (*bits < count) {
    if (!*size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    *dataword = (*dataword << 8) | **data;
    *bits += 8;
    while (**data == 0xff) {
      ++ *data;
      -- *size;
      if (!*size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
    ++ *data;
    -- *size;
  }
  *bits -= count;
  uint32_t result = *dataword >> *bits;
  *dataword &= ((uint32_t) 1 << *bits) - 1;
  return result;
}

static inline uint64_t color_from_floats (double red, double green, double blue, double alpha) {
  uint64_t outred = (red >= 0) ? red + 0.5 : 0;
  if (outred >= 0x10000u) outred = 0xffffu;
  uint64_t outgreen = (green >= 0) ? green + 0.5 : 0;
  if (outgreen >= 0x10000u) outgreen = 0xffffu;
  uint64_t outblue = (blue >= 0) ? blue + 0.5 : 0;
  if (outblue >= 0x10000u) outblue = 0xffffu;
  uint64_t outalpha = (alpha >= 0) ? alpha + 0.5 : 0;
  if (outalpha >= 0x10000u) outalpha = 0xffffu;
  return (outalpha << 48) | (outblue << 32) | (outgreen << 16) | outred;
}

static inline int16_t make_signed_16 (uint16_t value) {
  // this is a no-op (since int16_t must use two's complement), but it's necessary to avoid undefined behavior
  return (value >= 0x8000u) ? -(int16_t) bitnegate(value) - 1 : value;
}

static inline unsigned bit_width (uintmax_t value) {
  unsigned result;
  for (result = 0; value; result ++) value >>= 1;
  return result;
}

static inline bool is_whitespace (unsigned char value) {
  // checks if value is 0 or isspace(value), but independent of current locale and system encoding
  return !value || (value >= 9 && value <= 13) || value == 32;
}
