#include "proto.h"

#define PNG_MAX_LOOKBACK_COUNT 16

unsigned char * compress_PNG_data (struct context * context, const unsigned char * restrict data, size_t size, size_t extra, size_t * restrict output_size) {
  // extra is the number of zero bytes inserted before the compressed data; they are not included in the size
  unsigned char * output = ctxmalloc(context, extra + 8); // two bytes extra to handle leftover bits in dataword
  memset(output, 0, extra);
  size_t p, inoffset = 0, outoffset = extra + byteappend(output + extra, 0x78, 0x5e);
  uint16_t * references = ctxmalloc(context, sizeof *references * 0x8000u * PNG_MAX_LOOKBACK_COUNT);
  for (p = 0; p < ((size_t) 0x8000u * PNG_MAX_LOOKBACK_COUNT); p ++) references[p] = 0xffffu;
  uint32_t dataword = 0;
  uint8_t bits = 0;
  int force = 0;
  while (inoffset < size) {
    size_t blocksize, count;
    struct compressed_PNG_code * compressed = generate_compressed_PNG_block(context, data, inoffset, size, references, &blocksize, &count, force);
    force = 0;
    if (compressed) {
      inoffset += blocksize;
      if (inoffset == size) dataword |= 1 << bits;
      bits ++;
      unsigned char * compressed_data = emit_PNG_compressed_block(context, compressed, count, count >= 16, &blocksize, &dataword, &bits);
      if ((SIZE_MAX - outoffset) < (blocksize + 6)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
      output = ctxrealloc(context, output, outoffset + blocksize + 6);
      memcpy(output + outoffset, compressed_data, blocksize);
      ctxfree(context, compressed_data);
      outoffset += blocksize;
    }
    if (inoffset >= size) break;
    blocksize = compute_uncompressed_PNG_block_size(data, inoffset, size, references);
    if (blocksize >= 32) {
      if (blocksize > 0xffffu) blocksize = 0xffffu;
      if ((inoffset + blocksize) == size) dataword |= 1 << bits;
      bits += 3;
      while (bits) {
        output[outoffset ++] = dataword;
        dataword >>= 8;
        bits = (bits >= 8) ? bits - 8 : 0;
      }
      if ((SIZE_MAX - outoffset) < (blocksize + 10)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
      output = ctxrealloc(context, output, outoffset + blocksize + 10);
      write_le16_unaligned(output + outoffset, blocksize);
      write_le16_unaligned(output + outoffset + 2, 0xffffu - blocksize);
      memcpy(output + outoffset + 4, data + inoffset, blocksize);
      outoffset += blocksize + 4;
      inoffset += blocksize;
    } else
      force = 1;
  }
  ctxfree(context, references);
  while (bits) {
    output[outoffset ++] = dataword;
    dataword >>= 8;
    bits = (bits >= 8) ? bits - 8 : 0;
  }
  write_be32_unaligned(output + outoffset, compute_Adler32_checksum(data, size));
  *output_size = outoffset + 4 - extra;
  return output;
}

struct compressed_PNG_code * generate_compressed_PNG_block (struct context * context, const unsigned char * restrict data, size_t offset, size_t size,
                                                            uint16_t * restrict references, size_t * restrict blocksize, size_t * restrict count, int force) {
  size_t backref, current_offset = offset, allocated = 256;
  struct compressed_PNG_code * codes = ctxmalloc(context, allocated * sizeof *codes);
  *count = 0;
  int literals = 0, score = 0;
  unsigned length;
  while (((size - current_offset) >= 3) && ((size - current_offset) < (SIZE_MAX >> 4)))
    if (length = find_PNG_reference(data, references, current_offset, size, &backref)) {
      // we found a matching back reference, so emit any pending literals and the reference
      for (; literals; literals --) emit_PNG_code(context, &codes, &allocated, count, data[current_offset - literals], 0);
      emit_PNG_code(context, &codes, &allocated, count, -(int) length, current_offset - backref);
      score -= length - 1;
      if (score < 0) score = 0;
      for (; length; length --) append_PNG_reference(data, current_offset ++, references);
    } else {
      // no back reference: increase the pending literal count, and stop compressing data if a threshold is exceeded
      literals ++;
      score ++;
      append_PNG_reference(data, current_offset ++, references);
      if (score >= 64)
        if (force && (*count < 16))
          score = 0;
        else
          break;
    }
  if ((size - current_offset) < 3) {
    literals += size - current_offset;
    current_offset = size;
  }
  *blocksize = current_offset - offset;
  if ((force && (*blocksize < 32)) || ((*blocksize >= 32) && (score < 64)))
    for (; literals; literals --) emit_PNG_code(context, &codes, &allocated, count, data[current_offset - literals], 0);
  else
    *blocksize -= literals;
  if ((*blocksize < 32) && !force) {
    ctxfree(context, codes);
    return NULL;
  }
  return codes;
}

size_t compute_uncompressed_PNG_block_size (const unsigned char * restrict data, size_t offset, size_t size, uint16_t * restrict references) {
  size_t current_offset = offset;
  unsigned length, score = 0;
  for (; ((size - current_offset) >= 3) && ((size - current_offset) < 0xffffu); current_offset ++) {
    if (length = find_PNG_reference(data, references, current_offset, size, NULL)) {
      score += length - 1;
      if (score >= 16) break;
    } else if (score > 0)
      score --;
    append_PNG_reference(data, current_offset, references);
  }
  if ((size - current_offset) < 3) current_offset = size;
  return current_offset - offset;
}

unsigned find_PNG_reference (const unsigned char * data, const uint16_t * references, size_t current_offset, size_t size, size_t * restrict reference_offset) {
  uint_fast32_t search = compute_PNG_reference_key(data + current_offset) * (uint_fast32_t) PNG_MAX_LOOKBACK_COUNT;
  size_t backref, found;
  unsigned p, length, best = 0;
  for (p = 0; (p < PNG_MAX_LOOKBACK_COUNT) && (references[search + p] != 0xffffu); p ++) {
    backref = (current_offset & bitnegate(0x7fff)) | references[search + p];
    if (backref >= current_offset)
      if (current_offset < 0x8000u)
        continue;
      else
        backref -= 0x8000u;
    if (!memcmp(data + current_offset, data + backref, 3)) {
      for (length = 3; (length < 258) && ((current_offset + length) < size); length ++) if (data[current_offset + length] != data[backref + length]) break;
      if (length > best) {
        found = backref;
        best = length;
        if (best == 258) break;
      }
    }
  }
  if (best && reference_offset) *reference_offset = found;
  return best;
}

void append_PNG_reference (const unsigned char * restrict data, size_t offset, uint16_t * restrict references) {
  uint_fast32_t key = compute_PNG_reference_key(data + offset) * (uint_fast32_t) PNG_MAX_LOOKBACK_COUNT;
  memmove(references + key + 1, references + key, (PNG_MAX_LOOKBACK_COUNT - 1) * sizeof *references);
  references[key] = offset & 0x7fff;
}

uint16_t compute_PNG_reference_key (const unsigned char * data) {
  // should return a value between 0 and 0x7fff computed from the first three bytes of data
  uint_fast32_t key = (uint_fast32_t) *data | ((uint_fast32_t) data[1] << 8) | ((uint_fast32_t) data[2] << 16);
  // easy way out of a hash code: do a few iterations of a simple linear congruential RNG and return the upper bits of the final state
  unsigned p;
  for (p = 0; p < 3; p ++) key = 0x41c64e6du * key + 12345;
  return (key >> 17) & 0x7fff;
}

#undef PNG_MAX_LOOKBACK_COUNT

void emit_PNG_code (struct context * context, struct compressed_PNG_code ** codes, size_t * restrict allocated, size_t * restrict count, int code, unsigned ref) {
  // code >= 0 = literal; code < 0 = -length
  if (*count >= *allocated) {
    *allocated <<= 1;
    *codes = ctxrealloc(context, *codes, *allocated * sizeof **codes);
  }
  struct compressed_PNG_code result;
  if (code >= 0)
    result = (struct compressed_PNG_code) {.datacode = code};
  else {
    code = -code;
    // one extra entry to make looking codes up easier
    static const uint_fast16_t lengths[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 259};
    static const uint_fast16_t distances[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                              8193, 12289, 16385, 24577, 32769};
    for (result.datacode = 0; lengths[result.datacode + 1] <= code; result.datacode ++);
    result.dataextra = code - lengths[result.datacode];
    result.datacode += 0x101;
    for (result.distcode = 0; distances[result.distcode + 1] <= ref; result.distcode ++);
    result.distextra = ref - distances[result.distcode];
  }
  (*codes)[(*count) ++] = result;
}

unsigned char * emit_PNG_compressed_block (struct context * context, const struct compressed_PNG_code * restrict codes, size_t count, int custom_tree,
                                           size_t * restrict blocksize, uint32_t * restrict dataword, uint8_t * restrict bits) {
  // emit the code identifying whether the block is compressed with a fixed or custom tree
  *dataword |= (custom_tree ? 2 : 1) << *bits;
  *bits += 2;
  // count up the frequency of each code; this will be used to generate a custom tree (if needed) and to precalculate the output size
  size_t codecounts[0x120] = {[0x100] = 1}; // other entries will be zero-initialized
  size_t distcounts[0x20] = {0};
  size_t p, outsize;
  for (p = 0; p < count; p ++) {
    codecounts[codes[p].datacode] ++;
    if (codes[p].datacode > 0x100) distcounts[codes[p].distcode] ++;
  }
  unsigned char codelengths[0x120];
  unsigned char distlengths[0x20];
  unsigned char * output = NULL;
  *blocksize = 0;
  // ensure that we have the proper tree: use the documented tree if fixed, or generate (and output) a custom tree if custom
  if (custom_tree)
    output = generate_PNG_Huffman_trees(context, dataword, bits, blocksize, codecounts, distcounts, codelengths, distlengths);
  else {
    bytewrite(codelengths,
             //         00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
             /* 0x000 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
             /* 0x020 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
             /* 0x040 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
             /* 0x060 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
             /* 0x080 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
             /* 0x0a0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
             /* 0x0c0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
             /* 0x0e0 */ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
             /* 0x100 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8
    );
    memset(distlengths, 5, sizeof distlengths);
  }
  // precalculate the output size and allocate enough space for the output (and a little extra); this must account for parameter size too
  outsize = 7; // for rounding up
  for (p = 0; p < 0x11e; p ++) {
    uint_fast8_t valuesize = codelengths[p];
    if ((p >= 0x109) && (p < 0x11d)) valuesize += (p - 0x105) >> 2;
    if (!valuesize) continue;
    if (((codecounts[p] * valuesize / valuesize) != codecounts[p]) || ((SIZE_MAX - outsize) < (codecounts[p] * valuesize)))
      throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
    outsize += codecounts[p] * valuesize;
  }
  for (p = 0; p < 30; p ++) {
    uint_fast8_t valuesize = distlengths[p];
    if (p >= 4) valuesize += (p - 2) >> 1;
    if (!valuesize) continue;
    if (((distcounts[p] * valuesize / valuesize) != distcounts[p]) || ((SIZE_MAX - outsize) < (distcounts[p] * valuesize)))
      throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
    outsize += distcounts[p] * valuesize;
  }
  outsize >>= 3;
  output = ctxrealloc(context, output, *blocksize + outsize + 4);
  // build the actual encoded values from the tree lengths, properly sorted
  unsigned short outcodes[0x120];
  unsigned short outdists[0x20];
  generate_Huffman_codes(outcodes, sizeof outcodes / sizeof *outcodes, codelengths, 1);
  generate_Huffman_codes(outdists, sizeof outdists / sizeof *outdists, distlengths, 1);
  // and output all of the codes in order, ending with a 0x100 code
  #define flush while (*bits >= 8) output[(*blocksize) ++] = *dataword, *dataword >>= 8, *bits -= 8
  while (count --) {
    *dataword |= (size_t) outcodes[codes -> datacode] << *bits;
    *bits += codelengths[codes -> datacode];
    flush;
    if (codes -> datacode > 0x100) {
      if ((codes -> datacode >= 0x109) && (codes -> datacode < 0x11d)) {
        *dataword |= (size_t) codes -> dataextra << *bits;
        *bits += (codes -> datacode - 0x105) >> 2;
        // defer the flush because it can't overflow yet
      }
      *dataword |= (size_t) outdists[codes -> distcode] << *bits;
      *bits += distlengths[codes -> distcode];
      flush;
      if (codes -> distcode >= 4) {
        *dataword |= (size_t) codes -> distextra << *bits;
        *bits += (codes -> distcode - 2) >> 1;
        flush;
      }
    }
    codes ++;
  }
  *dataword |= (size_t) outcodes[0x100] << *bits;
  *bits += codelengths[0x100];
  flush;
  #undef flush
  return output;
}

unsigned char * generate_PNG_Huffman_trees (struct context * context, uint32_t * restrict dataword, uint8_t * restrict bits, size_t * restrict size,
                                            const size_t codecounts[restrict static 0x120], const size_t distcounts[restrict static 0x20],
                                            unsigned char codelengths[restrict static 0x120], unsigned char distlengths[restrict static 0x20]) {
  // this function will generate trees, discard them and only preserve the lengths; that way, the real (properly ordered) trees can be rebuilt later
  // also outputs the tree length data to the output stream and returns it
  generate_Huffman_tree(context, codecounts, codelengths, 0x120, 15);
  generate_Huffman_tree(context, distcounts, distlengths, 0x20, 15);
  unsigned char lengths[0x140];
  unsigned char encoded[0x140];
  unsigned repcount, maxcode, maxdist, encodedlength = 0, p = 0;
  for (maxcode = 0x11f; !codelengths[maxcode]; maxcode --);
  for (maxdist = 0x1f; maxdist && !distlengths[maxdist]; maxdist --);
  memcpy(lengths, codelengths, maxcode + 1);
  memcpy(lengths + maxcode + 1, distlengths, maxdist + 1);
  while (p < (maxcode + maxdist + 2))
    if (!lengths[p]) {
      for (repcount = 1; (repcount < 0x8a) && ((p + repcount) < (maxcode + maxdist + 2)) && !lengths[p + repcount]; repcount ++);
      if (repcount < 3) {
        encoded[encodedlength ++] = 0;
        p ++;
      } else {
        p += repcount;
        encoded[encodedlength ++] = 17 + (repcount > 10);
        encoded[encodedlength ++] = repcount - ((repcount >= 11) ? 11 : 3);
      }
    } else if (p && (lengths[p] == lengths[p - 1])) {
      for (repcount = 1; (repcount < 6) && ((p + repcount) < (maxcode + maxdist + 2)) && (lengths[p + repcount] == lengths[p - 1]); repcount ++);
      if (repcount < 3)
        encoded[encodedlength ++] = lengths[p ++];
      else {
        encoded[encodedlength ++] = 16;
        encoded[encodedlength ++] = repcount - 3;
        p += repcount;
      }
    } else
      encoded[encodedlength ++] = lengths[p ++];
  size_t encodedcounts[19] = {0};
  for (p = 0; p < encodedlength; p ++) {
    encodedcounts[encoded[p]] ++;
    if (encoded[p] >= 16) p ++;
  }
  generate_Huffman_tree(context, encodedcounts, lengths, 19, 7);
  unsigned short codes[19];
  static const unsigned char codeorder[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  for (repcount = 18; (repcount > 3) && !lengths[codeorder[repcount]]; repcount --);
  generate_Huffman_codes(codes, 19, lengths, 1);
  *dataword |= (maxcode & 0x1f) << *bits;
  *bits += 5;
  *dataword |= maxdist << *bits;
  *bits += 5;
  *dataword |= (repcount - 3) << *bits;
  *bits += 4;
  unsigned char * result = ctxmalloc(context, 0x100);
  unsigned char * current = result;
  #define flush while (*bits >= 8) *(current ++) = *dataword, *dataword >>= 8, *bits -= 8
  flush;
  for (p = 0; p <= repcount; p ++) {
    *dataword |= lengths[codeorder[p]] << *bits;
    *bits += 3;
    flush;
  }
  for (p = 0; p < encodedlength; p ++) {
    *dataword |= codes[encoded[p]] << *bits;
    *bits += lengths[encoded[p]];
    if (encoded[p] >= 16) {
      uint_fast8_t repeattype = encoded[p] - 16;
      *dataword |= encoded[++ p] << *bits;
      *bits += repeattype[(const unsigned char []) {2, 3, 7}];
    }
    flush;
  }
  #undef flush
  *size = current - result;
  return result;
}
