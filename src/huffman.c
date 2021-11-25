#include "proto.h"

void generate_Huffman_tree (struct context * context, const size_t * restrict counts, unsigned char * restrict lengths, size_t entries, unsigned char max) {
  size_t * sorted = ctxmalloc(context, 2 * entries * sizeof *sorted);
  size_t p, truecount = 0;
  for (p = 0; p < entries; p ++) if (counts[p]) {
    sorted[2 * truecount] = p;
    sorted[2 * truecount + 1] = ~counts[p];
    truecount ++;
  }
  memset(lengths, 0, entries);
  if (truecount < 2) {
    if (truecount) lengths[*sorted] = 1;
    goto done;
  }
  qsort(sorted, truecount, 2 * sizeof *sorted, &compare_index_value_pairs);
  size_t * parents = ctxmalloc(context, (entries + truecount) * sizeof *parents);
  size_t * pendingnodes = ctxmalloc(context, truecount * sizeof *pendingnodes);
  size_t * pendingcounts = ctxmalloc(context, truecount * sizeof *pendingcounts);
  size_t sum, next = entries, remaining = truecount;
  for (p = 0; p < truecount; p ++) {
    pendingnodes[p] = sorted[2 * p];
    pendingcounts[p] = counts[pendingnodes[p]];
  }
  while (remaining > 1) {
    parents[pendingnodes[-- remaining]] = next;
    parents[pendingnodes[remaining - 1]] = next;
    sum = pendingcounts[remaining - 1] + pendingcounts[remaining];
    size_t first = 0, last = remaining - 1;
    while (first < last) {
      p = (first + last) >> 1;
      if (sum >= pendingcounts[p])
        last = p;
      else if (last > (first + 1))
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
  unsigned char length;
  sum = 0; // reuse it to track the current maximum length
  for (p = 0; p < truecount; p ++) {
    next = sorted[p * 2];
    length = 0;
    while (next != root) {
      if (length < 0xff) length ++;
      next = parents[next];
    }
    lengths[sorted[p * 2]] = length;
    if (length > sum) sum = length;
  }
  ctxfree(context, parents);
  if (sum <= max) goto done;
  // the maximum length has been exceeded, so increase some other lengths to make everything fit
  remaining = (size_t) 1 << max;
  for (p = 0; p < truecount; p ++) {
    next = sorted[p * 2];
    if (lengths[next] > max) {
      lengths[next] = max;
      remaining --;
    } else {
      while (((size_t) 1 << (max - lengths[next])) > remaining) lengths[next] ++;
      while ((remaining - ((size_t) 1 << (max - lengths[next]))) < (truecount - p - 1)) lengths[next] ++;
      remaining -= (size_t) 1 << (max - lengths[next]);
    }
  }
  for (p = 0; remaining; p ++) {
    next = sorted[p * 2];
    while ((lengths[next] > 1) && (remaining >= ((size_t) 1 << (max - lengths[next])))) {
      remaining -= (size_t) 1 << (max - lengths[next]);
      lengths[next] --;
    }
  }
  done:
  ctxfree(context, sorted);
}

void generate_Huffman_codes (unsigned short * restrict codes, unsigned count, const unsigned char * restrict lengths, int invert) {
  // generates codes in ascending order: shorter codes before longer codes, and for the same length, smaller values before larger values
  unsigned p, remaining = 0;
  for (p = 0; p < count; p ++) if (lengths[p]) remaining ++;
  uint_fast8_t bits, length = 0;
  uint_fast16_t temp, code = 0;
  // note that p = count at the start!
  for (; remaining; p ++) {
    if (p >= count) {
      length ++;
      code <<= 1;
      p = 0;
    }
    if (lengths[p] != length) continue;
    // invert the code so it can be written out directly
    if (invert) {
      temp = code ++;
      codes[p] = 0;
      for (bits = 0; bits < length; bits ++) {
        codes[p] = (codes[p] << 1) | (temp & 1);
        temp >>= 1;
      }
    } else
      codes[p] = code ++;
    remaining --;
  }
}
