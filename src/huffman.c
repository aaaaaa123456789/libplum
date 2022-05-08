#include "proto.h"

void generate_Huffman_tree (struct context * context, const size_t * restrict counts, unsigned char * restrict lengths, size_t entries, unsigned char max) {
  uint64_t * sorted = ctxmalloc(context, 2 * entries * sizeof *sorted);
  size_t truecount = 0;
  for (size_t p = 0; p < entries; p ++) if (counts[p]) {
    sorted[2 * truecount] = p;
    sorted[2 * truecount + 1] = ~(uint64_t) counts[p];
    truecount ++;
  }
  memset(lengths, 0, entries);
  if (truecount < 2) {
    if (truecount) lengths[*sorted] = 1;
    ctxfree(context, sorted);
    return;
  }
  qsort(sorted, truecount, 2 * sizeof *sorted, &compare_index_value_pairs);
  size_t * parents = ctxmalloc(context, (entries + truecount) * sizeof *parents);
  size_t * pendingnodes = ctxmalloc(context, truecount * sizeof *pendingnodes);
  size_t * pendingcounts = ctxmalloc(context, truecount * sizeof *pendingcounts);
  size_t next = entries;
  for (size_t p = 0; p < truecount; p ++) {
    pendingnodes[p] = sorted[2 * p];
    pendingcounts[p] = counts[pendingnodes[p]];
  }
  uint64_t remaining = truecount;
  while (remaining > 1) {
    parents[pendingnodes[-- remaining]] = next;
    parents[pendingnodes[remaining - 1]] = next;
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
    next = sorted[p * 2];
    unsigned char length = 0;
    while (next != root) {
      if (length < 0xff) length ++;
      next = parents[next];
    }
    lengths[sorted[p * 2]] = length;
    if (length > maxlength) maxlength = length;
  }
  ctxfree(context, parents);
  if (maxlength > max) {
    // the maximum length has been exceeded, so increase some other lengths to make everything fit
    remaining = (uint64_t) 1 << max;
    for (size_t p = 0; p < truecount; p ++) {
      next = sorted[p * 2];
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
      next = sorted[p * 2];
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
