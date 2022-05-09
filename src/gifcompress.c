#include "proto.h"

unsigned char * compress_GIF_data (struct context * context, const unsigned char * restrict data, size_t count, size_t * length, unsigned codesize) {
  struct compressed_GIF_code * codes = ctxmalloc(context, sizeof *codes * 4097);
  initialize_GIF_compression_codes(codes, codesize);
  *length = 0;
  size_t allocated = 254; // initial size
  unsigned char * output = ctxmalloc(context, allocated);
  unsigned current_codesize = codesize + 1, bits = current_codesize, max_code = (1 << codesize) + 1, current_code = *(data ++);
  uint_fast32_t chain = 1, codeword = 1 << codesize;
  uint_fast8_t shortchains = 0;
  while (-- count) {
    uint_fast8_t search = *(data ++);
    uint_fast16_t p;
    for (p = 0; p <= max_code; p ++) if (!codes[p].type && codes[p].reference == current_code && codes[p].value == search) break;
    if (p <= max_code) {
      current_code = p;
      chain ++;
    } else {
      codeword |= current_code << bits;
      bits += current_codesize;
      codes[++ max_code] = (struct compressed_GIF_code) {.type = 0, .reference = current_code, .value = search};
      current_code = search;
      if (current_codesize > codesize + 2)
        if (chain <= current_codesize / codesize)
          shortchains ++;
        else if (shortchains)
          shortchains --;
      chain = 1;
      if (max_code > 4095) max_code = 4095;
      if (max_code == (1 << current_codesize)) current_codesize ++;
      if (shortchains > codesize + 8) {
        // output a clear code and reset the code table
        codeword |= 1 << (bits + codesize);
        bits += current_codesize;
        max_code = (1 << codesize) + 1;
        current_codesize = codesize + 1;
        shortchains = 0;
      }
    }
    while (bits >= 8) {
      if (allocated == *length) output = ctxrealloc(context, output, allocated <<= 1);
      output[(*length) ++] = codeword;
      codeword >>= 8;
      bits -= 8;
    }
  }
  codeword |= current_code << bits;
  bits += current_codesize;
  codeword |= ((1 << codesize) + 1) << bits;
  bits += current_codesize;
  while (bits) {
    if (allocated == *length) output = ctxrealloc(context, output, allocated += 4);
    output[(*length) ++] = codeword;
    codeword >>= 8;
    bits = (bits > 8) ? bits - 8 : 0;
  }
  ctxfree(context, codes);
  return output;
}

void decompress_GIF_data (struct context * context, unsigned char * restrict result, const unsigned char * restrict source, size_t expected_length,
                          size_t length, unsigned codesize) {
  struct compressed_GIF_code * codes = ctxmalloc(context, sizeof *codes * 4097);
  initialize_GIF_compression_codes(codes, codesize);
  unsigned bits = 0, current_codesize = codesize + 1, max_code = (1 << codesize) + 1;
  uint_fast32_t codeword = 0;
  int lastcode = -1;
  unsigned char * current = result;
  unsigned char * limit = result + expected_length;
  while (true) {
    while (bits < current_codesize) {
      if (!(length --)) {
        // handle images that are so broken that they never emit a stop code
        if (current != limit) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        ctxfree(context, codes);
        return;
      }
      codeword |= (uint_fast32_t) *(source ++) << bits;
      bits += 8;
    }
    uint_fast16_t code = codeword & ((1u << current_codesize) - 1);
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
        if (code != max_code + 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
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

void initialize_GIF_compression_codes (struct compressed_GIF_code * restrict codes, unsigned codesize) {
  unsigned code;
  for (code = 0; code < (1 << codesize); code ++) codes[code] = (struct compressed_GIF_code) {.reference = -1, .value = code, .type = 0};
  codes[code ++] = (struct compressed_GIF_code) {.type = 1, .reference = -1};
  codes[code ++] = (struct compressed_GIF_code) {.type = 2, .reference = -1};
  for (; code < 4096; code ++) codes[code] = (struct compressed_GIF_code) {.type = 3, .reference = -1};
}

uint8_t find_leading_GIF_code (const struct compressed_GIF_code * restrict codes, unsigned code) {
  return (codes[code].reference < 0) ? codes[code].value : find_leading_GIF_code(codes, codes[code].reference);
}

void emit_GIF_data (struct context * context, const struct compressed_GIF_code * restrict codes, unsigned code, unsigned char ** result, unsigned char * limit) {
  if (codes[code].reference >= 0) emit_GIF_data(context, codes, codes[code].reference, result, limit);
  if (*result >= limit) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  *((*result) ++) = codes[code].value;
}
