#include "proto.h"

void generate_Huffman_tree (struct context * context, const size_t * restrict counts, unsigned char * restrict lengths, size_t entries, unsigned char max) {
  struct pair * sorted = ctxmalloc(context, entries * sizeof *sorted);
  size_t truecount = 0;
  for (size_t p = 0; p < entries; p ++) if (counts[p]) sorted[truecount ++] = (struct pair) {.value = p, .index = ~(uint64_t) counts[p]};
  memset(lengths, 0, entries);
  if (truecount < 2) {
    if (truecount) lengths[sorted -> value] = 1;
    ctxfree(context, sorted);
    return;
  }
  sort_pairs(sorted, truecount);
  size_t * pendingnodes = ctxmalloc(context, truecount * sizeof *pendingnodes);
  size_t * pendingcounts = ctxmalloc(context, truecount * sizeof *pendingcounts);
  for (size_t p = 0; p < truecount; p ++) {
    pendingnodes[p] = sorted[p].value;
    pendingcounts[p] = counts[sorted[p].value];
  }
  size_t next = entries;
  size_t * parents = ctxmalloc(context, (entries + truecount) * sizeof *parents);
  for (uint64_t remaining = truecount - 1; remaining; remaining --) {
    parents[pendingnodes[remaining]] = parents[pendingnodes[remaining - 1]] = next;
    size_t sum = pendingcounts[remaining - 1] + pendingcounts[remaining];
    size_t first = 0, last = remaining - 1;
    while (first < last) {
      size_t p = (first + last) >> 1;
      if (sum >= pendingcounts[p])
        last = p;
      else if (last > first + 1)
        first = p;
      else
        first = p + 1;
    }
    memmove(pendingnodes + first + 1, pendingnodes + first, sizeof *pendingnodes * (remaining - 1 - first));
    memmove(pendingcounts + first + 1, pendingcounts + first, sizeof *pendingcounts * (remaining - 1 - first));
    pendingnodes[first] = next ++;
    pendingcounts[first] = sum;
  }
  ctxfree(context, pendingcounts);
  ctxfree(context, pendingnodes);
  size_t root = next - 1;
  unsigned char maxlength = 0;
  for (size_t p = 0; p < truecount; p ++) {
    next = sorted[p].value;
    unsigned char length = 0;
    while (next != root) {
      if (length < 0xff) length ++;
      next = parents[next];
    }
    lengths[sorted[p].value] = length;
    if (length > maxlength) maxlength = length;
  }
  ctxfree(context, parents);
  if (maxlength > max) {
    // the maximum length has been exceeded, so increase some other lengths to make everything fit
    uint64_t remaining = (uint64_t) 1 << max;
    for (size_t p = 0; p < truecount; p ++) {
      next = sorted[p].value;
      if (lengths[next] > max) {
        lengths[next] = max;
        remaining --;
      } else {
        while (((uint64_t) 1 << (max - lengths[next])) > remaining) lengths[next] ++;
        while (remaining - ((uint64_t) 1 << (max - lengths[next])) < truecount - p - 1) lengths[next] ++;
        remaining -= (uint64_t) 1 << (max - lengths[next]);
      }
    }
    for (size_t p = 0; remaining; p ++) {
      next = sorted[p].value;
      while (lengths[next] > 1 && remaining >= ((uint64_t) 1 << (max - lengths[next]))) {
        remaining -= (uint64_t) 1 << (max - lengths[next]);
        lengths[next] --;
      }
    }
  }
  ctxfree(context, sorted);
}

void generate_Huffman_codes (unsigned short * restrict codes, size_t count, const unsigned char * restrict lengths, bool invert) {
  // generates codes in ascending order: shorter codes before longer codes, and for the same length, smaller values before larger values
  size_t remaining = 0;
  for (size_t p = 0; p < count; p ++) if (lengths[p]) remaining ++;
  uint_fast8_t length = 0;
  uint_fast16_t code = 0;
  for (size_t p = SIZE_MAX; remaining; p ++) {
    if (p >= count) {
      length ++;
      code <<= 1;
      p = 0;
    }
    if (lengths[p] != length) continue;
    if (invert) {
      // for some image formats, invert the code so it can be written out directly (first branch at the LSB)
      uint_fast16_t temp = code ++;
      codes[p] = 0;
      for (uint_fast8_t bits = 0; bits < length; bits ++) {
        codes[p] = (codes[p] << 1) | (temp & 1);
        temp >>= 1;
      }
    } else
      codes[p] = code ++;
    remaining --;
  }
}
