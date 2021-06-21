static inline noreturn throw (struct context * context, unsigned error) {
  context -> status = error;
  longjmp(context -> target, 1);
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

static inline uint16_t bitextend (uint16_t value, unsigned width) {
  uint32_t result = value;
  while (width < 16) {
    result |= result << width;
    width <<= 1;
  }
  return result >> (width - 16);
}

static inline void append_metadata (struct plum_image * image, struct plum_metadata * metadata) {
  metadata -> next = image -> metadata;
  image -> metadata = metadata;
}

static inline void * append_output_node (struct context * context, size_t size) {
  struct data_node * node = ctxmalloc(context, sizeof *node + size);
  *node = (struct data_node) {.size = size, .previous = context -> output, .next = NULL};
  if (context -> output) context -> output -> next = node;
  context -> output = node;
  return node -> data;
}

static inline int bit_depth_less_than (uint32_t depth, uint32_t target) {
  // formally "less than or equal to", but that would be a very long name
  return !((target - depth) & 0x80808080u);
}
