#include "proto.h"

void * decompress_PNG_data (struct context * context, const unsigned char * compressed, size_t size, size_t expected) {
  if (size <= 6) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  if ((*compressed & 0x8f) != 8 || (compressed[1] & 0x20) || read_be16_unaligned(compressed) % 31) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  // ignore the window size - treat it as 0x8000 for simpler code (everything will be in memory anyway)
  compressed += 2;
  size -= 6; // pretend the checksum is not part of the data
  unsigned char * decompressed = ctxmalloc(context, expected);
  size_t current = 0;
  bool last_block;
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
        if (literalcount > expected - current) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        if (literalcount > size) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        memcpy(decompressed + current, compressed, literalcount);
        current += literalcount;
        compressed += literalcount;
        size -= literalcount;
      } break;
      case 1:
        decompress_PNG_block(context, &compressed, decompressed, &size, &current, expected, &dataword, &bits, default_PNG_Huffman_table_lengths);
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
  if (size || current != expected) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
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
  for (uint_fast8_t p = 0; p < lengths; p ++) internal_sizes[compressed_PNG_code_table_order[p]] = shift_in_left(context, 3, dataword, bits, compressed, size);
  short * tree = decode_PNG_Huffman_tree(context, internal_sizes, sizeof internal_sizes);
  if (!tree) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  uint_fast16_t index = 0;
  while (index < literals + distances) {
    uint_fast8_t code = next_PNG_Huffman_code(context, tree, compressed, size, dataword, bits);
    switch (code) {
      case 16: {
        if (!index) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        uint_fast8_t codesize = codesizes[index - 1], count = 3 + shift_in_left(context, 2, dataword, bits, compressed, size);
        if (index + count > literals + distances) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        while (count --) codesizes[index ++] = codesize;
      } break;
      case 17: case 18: {
        uint_fast8_t count = ((code == 18) ? 11 : 3) + shift_in_left(context, (code == 18) ? 7 : 3, dataword, bits, compressed, size);
        if (index + count > literals + distances) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
        while (count --) codesizes[index ++] = 0;
      } break;
      default:
        codesizes[index ++] = code;
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
  while (true) {
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
    uint_fast16_t length = compressed_PNG_base_lengths[code];
    uint_fast8_t lengthbits = compressed_PNG_length_bits[code];
    if (lengthbits) length += shift_in_left(context, lengthbits, dataword, bits, compressed, size);
    uint_fast8_t distcode = next_PNG_Huffman_code(context, disttree, compressed, size, dataword, bits);
    if (distcode > 29) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    uint_fast16_t distance = compressed_PNG_base_distances[distcode];
    uint_fast8_t distbits = compressed_PNG_distance_bits[distcode];
    if (distbits) distance += shift_in_left(context, distbits, dataword, bits, compressed, size);
    if (distance > *current) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    if (*current + length > expected || *current + length < *current) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
    for (; length; -- length, ++ *current) decompressed[*current] = decompressed[*current - distance];
  }
  ctxfree(context, disttree);
  ctxfree(context, codetree);
}

short * decode_PNG_Huffman_tree (struct context * context, const unsigned char * codesizes, unsigned count) {
  // root at index 0; each non-leaf node takes two entries (index for the 0 branch, index+1 for the 1 branch)
  // non-negative value: branch points to a leaf node; negative value: branch points to another non-leaf at -index
  // -1 is used as an invalid value, since -1 cannot ever occur (as index 1 would overlap with the root)
  uint_fast16_t total = 0;
  uint_fast8_t codelength = 0;
  for (uint_fast16_t p = 0; p < count; p ++) if (codesizes[p]) {
    total ++;
    if (codesizes[p] > codelength) codelength = codesizes[p];
  }
  if (!total) return NULL;
  uint_fast16_t maxlength = count * 2 * codelength;
  short * result = ctxmalloc(context, maxlength * sizeof *result);
  for (uint_fast16_t p = 0; p < maxlength; p ++) result[p] = -1;
  uint_fast16_t code = 0;
  short last = 2;
  for (uint_fast8_t curlength = 1; curlength <= codelength; curlength ++) {
    code <<= 1;
    for (uint_fast16_t p = 0; p < count; p ++) if (codesizes[p] == curlength) {
      if (code >= (1u << curlength)) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
      uint_fast16_t index = 0;
      for (uint_fast8_t bit = curlength - 1; bit; bit --) {
        if (code & (1u << bit)) index ++;
        if (result[index] == -1) {
          result[index] = -last;
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
  return ctxrealloc(context, result, last * sizeof *result);
}

uint16_t next_PNG_Huffman_code (struct context * context, const short * restrict tree, const unsigned char ** compressed, size_t * restrict size,
                                uint32_t * restrict dataword, uint8_t * restrict bits) {
  short index = 0;
  while (true) {
    index += shift_in_left(context, 1, dataword, bits, compressed, size);
    if (tree[index] >= 0) return tree[index];
    index = -tree[index];
    if (index == 1) throw(context, PLUM_ERR_INVALID_FILE_FORMAT);
  }
}
