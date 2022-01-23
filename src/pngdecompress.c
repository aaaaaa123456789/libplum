#include "proto.h"

void * decompress_PNG_data (struct context * context, const unsigned char * compressed, size_t size, size_t expected) {
  if (size <= 6) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (((*compressed & 0x8f) != 8) || (compressed[1] & 0x20) || (read_be16_unaligned(compressed) % 31)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // ignore the window size - treat it as 0x8000 for simpler code (everything will be in memory anyway)
  compressed += 2;
  size -= 6; // pretend the checksum is not part of the data
  unsigned char * decompressed = ctxmalloc(context, expected);
  size_t current = 0;
  int last_block;
  uint32_t dataword = 0;
  uint8_t bits = 0;
  do {
    last_block = shift_in_left(context, 1, &dataword, &bits, &compressed, &size);
    switch (shift_in_left(context, 2, &dataword, &bits, &compressed, &size)) {
      case 0: {
        dataword >>= bits & 7;
        bits &= ~7;
        uint32_t literalcount = shift_in_left(context, 32, &dataword, &bits, &compressed, &size);
        if (((literalcount >> 16) ^ (literalcount & 0xffffu)) != 0xffffu) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        literalcount &= 0xffffu;
        if (literalcount > (expected - current)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (literalcount > size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        memcpy(decompressed + current, compressed, literalcount);
        current += literalcount;
        compressed += literalcount;
        size -= literalcount;
      } break;
      case 1:
        decompress_PNG_block(context, &compressed, decompressed, &size, &current, expected, &dataword, &bits, (const unsigned char []) {
          //         00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
          /* 0x000 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
          /* 0x020 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
          /* 0x040 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
          /* 0x060 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
          /* 0x080 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
          /* 0x0a0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
          /* 0x0c0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
          /* 0x0e0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
          /* 0x100 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
          /* 0x120 */ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
        });
        break;
      case 2: {
        unsigned char codesizes[0x140];
        extract_PNG_code_table(context, &compressed, &size, codesizes, &dataword, &bits);
        decompress_PNG_block(context, &compressed, decompressed, &size, &current, expected, &dataword, &bits, codesizes);
      } break;
      default:
        throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    }
  } while (!last_block);
  if (size || (current != expected)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if (compute_Adler32_checksum(decompressed, expected) != read_be32_unaligned(compressed)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  return decompressed;
}

void extract_PNG_code_table (struct context * context, const unsigned char ** compressed, size_t * restrict size, unsigned char codesizes[restrict static 0x140],
                             uint32_t * restrict dataword, uint8_t * restrict bits) {
  uint_fast16_t header = shift_in_left(context, 14, dataword, bits, compressed, size);
  unsigned literals = 0x101 + (header & 0x1f);
  unsigned distances = 1 + ((header >> 5) & 0x1f);
  unsigned lengths = 4 + (header >> 10);
  unsigned char internal_sizes[19] = {0};
  unsigned p, count;
  for (p = 0; p < lengths; p ++) internal_sizes[p[(const unsigned char []) {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15}]] =
    shift_in_left(context, 3, dataword, bits, compressed, size);
  short * tree = decode_PNG_Huffman_tree(context, internal_sizes, sizeof internal_sizes);
  if (!tree) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  p = 0;
  while (p < (literals + distances)) {
    uint_fast8_t code = next_PNG_Huffman_code(context, tree, compressed, size, dataword, bits);
    switch (code) {
      case 16:
        if (!p) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        count = 3 + shift_in_left(context, 2, dataword, bits, compressed, size);
        if ((p + count) > (literals + distances)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        code = codesizes[p - 1];
        while (count --) codesizes[p ++] = code;
        break;
      case 17: case 18:
        count = ((code == 18) ? 11 : 3) + shift_in_left(context, (code == 18) ? 7 : 3, dataword, bits, compressed, size);
        if ((p + count) > (literals + distances)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        while (count --) codesizes[p ++] = 0;
        break;
      default:
        codesizes[p ++] = code;
    }
  }
  ctxfree(context, tree);
  if (literals < 0x120) memmove(codesizes + 0x120, codesizes + literals, distances);
  memset(codesizes + literals, 0, 0x120 - literals);
  memset(codesizes + 0x120 + distances, 0, 0x20 - distances);
}

void decompress_PNG_block (struct context * context, const unsigned char ** compressed, unsigned char * restrict decompressed, size_t * restrict size,
                           size_t * restrict current, size_t expected, uint32_t * restrict dataword, uint8_t * restrict bits,
                           const unsigned char codesizes[restrict static 0x140]) {
  // a single list of codesizes for all codes: 0x00-0xff for literals, 0x100 for end of codes, 0x101-0x11d for lengths, 0x120-0x13d for distances
  short * codetree = decode_PNG_Huffman_tree(context, codesizes, 0x120);
  if (!codetree) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  short * disttree = decode_PNG_Huffman_tree(context, codesizes + 0x120, 0x20);
  while (1) {
    uint_fast16_t code = next_PNG_Huffman_code(context, codetree, compressed, size, dataword, bits);
    if (code >= 0x11e) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (code == 0x100) break;
    if (code < 0x100) {
      if (*current >= expected) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      decompressed[(*current) ++] = code;
      continue;
    }
    if (!disttree) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    code -= 0x101;
    uint_fast16_t length = code[(const uint_fast16_t []) {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195,
                                                          227, 258}];
    code = code[(const uint_fast16_t []) {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0}];
    if (code) length += shift_in_left(context, code, dataword, bits, compressed, size);
    code = next_PNG_Huffman_code(context, disttree, compressed, size, dataword, bits);
    if (code > 29) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint_fast16_t distance = code[(const uint_fast16_t []) {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                                            4097, 6145, 8193, 12289, 16385, 24577}];
    code = code[(const uint_fast16_t []) {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13}];
    if (code) distance += shift_in_left(context, code, dataword, bits, compressed, size);
    if (distance > *current) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (((*current + length) > expected) || ((*current + length) < *current)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    for (; length; -- length, ++ *current) decompressed[*current] = decompressed[*current - distance];
  }
  ctxfree(context, disttree);
  ctxfree(context, codetree);
}

short * decode_PNG_Huffman_tree (struct context * context, const unsigned char * codesizes, unsigned count) {
  // root at index 0; each non-leaf node takes two entries (index for the 0 branch, index+1 for the 1 branch)
  // non-negative value: branch points to a leaf node; negative value: branch points to another non-leaf at -index
  // -1 is used as an invalid value, since -1 cannot ever occur (as index 1 would overlap with the root)
  uint_fast16_t p, last, total = 0;
  uint_fast8_t codelength = 0;
  for (p = 0; p < count; p ++) if (codesizes[p]) {
    total ++;
    last = p;
    if (codesizes[p] > codelength) codelength = codesizes[p];
  }
  if (!total) return NULL;
  short * result = ctxmalloc(context, (count * 2 * codelength) * sizeof *result);
  for (p = 0; p < (count * 2 * codelength); p ++) result[p] = -1;
  uint_fast16_t index, curlength, code = 0;
  last = 2;
  for (curlength = 1; curlength <= codelength; curlength ++) {
    code <<= 1;
    for (p = 0; p < count; p ++) if (codesizes[p] == curlength) {
      if (code >= (1u << curlength)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      index = 0;
      for (total = curlength - 1; total; total --) {
        if (code & (1u << total)) index ++;
        if (result[index] == -1) {
          result[index] = -(short) last;
          last += 2;
        }
        index = -result[index];
      }
      if (code & 1) index ++;
      result[index] = p;
      code ++;
    }
  }
  if (code > (1u << codelength)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  return result;
}

uint16_t next_PNG_Huffman_code (struct context * context, const short * tree, const unsigned char ** compressed, size_t * restrict size,
                                uint32_t * restrict dataword, uint8_t * restrict bits) {
  short index = 0;
  while (1) {
    index += shift_in_left(context, 1, dataword, bits, compressed, size);
    if (tree[index] >= 0) return tree[index];
    index = -tree[index];
    if (index == 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}
