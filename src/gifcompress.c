#include "proto.h"

void decompress_GIF_data (struct context * context, unsigned char * restrict result, const unsigned char * source, size_t expected_length,
                          size_t length, unsigned codesize) {
  struct compressed_GIF_code * codes = ctxmalloc(context, sizeof *codes * 4097);
  initialize_GIF_compression_codes(codes, codesize);
  unsigned bits = 0, current_codesize = codesize + 1, max_code = (1 << codesize) + 1;
  uint_fast32_t code, codeword = 0;
  int lastcode = -1;
  unsigned char * current = result;
  unsigned char * limit = result + expected_length;
  while (1) {
    while (bits < current_codesize) {
      if (!(length --)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      codeword |= (uint_fast32_t) *(source ++) << bits;
      bits += 8;
    }
    code = codeword & (((uint_fast32_t) 1 << current_codesize) - 1);
    codeword >>= current_codesize;
    bits -= current_codesize;
    switch (codes[code].type) {
      case 0:
        emit_GIF_data(context, codes, code, &current, limit);
        if (lastcode >= 0)
          codes[++ max_code] = (struct compressed_GIF_code) {.reference = lastcode, .value = find_leading_GIF_code(codes, code), .type = 0};
        lastcode = code;
        break;
      case 1:
        initialize_GIF_compression_codes(codes, codesize);
        current_codesize = codesize + 1;
        max_code = (1 << codesize) + 1;
        lastcode = -1;
        break;
      case 2:
        if (current != limit) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        ctxfree(context, codes);
        return;
      case 3:
        if (code != (max_code + 1)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (lastcode < 0) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        codes[++ max_code] = (struct compressed_GIF_code) {.reference = lastcode, .value = find_leading_GIF_code(codes, lastcode), .type = 0};
        emit_GIF_data(context, codes, max_code, &current, limit);
        lastcode = code;
    }
    if (max_code >= 4095)
      max_code = 4095;
    else if (max_code == ((1 << current_codesize) - 1))
      current_codesize ++;
  }
}

void initialize_GIF_compression_codes (struct compressed_GIF_code * codes, unsigned codesize) {
  unsigned code;
  for (code = 0; code < (1 << codesize); code ++) codes[code] = (struct compressed_GIF_code) {.reference = -1, .value = code, .type = 0};
  codes[code ++] = (struct compressed_GIF_code) {.type = 1, .reference = -1};
  codes[code ++] = (struct compressed_GIF_code) {.type = 2, .reference = -1};
  for (; code < 4096; code ++) codes[code] = (struct compressed_GIF_code) {.type = 3, .reference = -1};
}

uint8_t find_leading_GIF_code (const struct compressed_GIF_code * codes, unsigned code) {
  if (codes[code].reference < 0) return codes[code].value;
  return find_leading_GIF_code(codes, codes[code].reference);
}

void emit_GIF_data (struct context * context, const struct compressed_GIF_code * codes, unsigned code, unsigned char ** result, unsigned char * limit) {
  if (codes[code].reference >= 0) emit_GIF_data(context, codes, codes[code].reference, result, limit);
  if (*result >= limit) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  *((*result) ++) = codes[code].value;
}
