#include "proto.h"

#define swap(first, second) do {    \
  uint64_t temp = first;            \
  first = second;                   \
  second = temp;                    \
} while (false)

void sort_values (uint64_t * restrict data, uint64_t count) {
  #define THRESHOLD 16
  uint64_t * buffer;
  if (count < THRESHOLD || !(buffer = malloc(count * sizeof *buffer))) {
    quicksort_values(data, count);
    return;
  }
  uint_fast64_t start = 0, runsize = 2;
  bool descending = data[1] < *data;
  for (uint_fast64_t current = 2; current < count; current ++)
    if (descending ? data[current] <= data[current - 1] : data[current] >= data[current - 1])
      runsize ++;
    else {
      if (descending && runsize >= THRESHOLD) for (uint_fast64_t p = 0; p < runsize / 2; p ++) swap(data[start + p], data[start + runsize - 1 - p]);
      buffer[start] = runsize;
      start = current ++;
      if (current == count)
        runsize = 1;
      else {
        descending = data[current] < data[current - 1];
        runsize = 2;
      }
    }
  if (descending && runsize >= THRESHOLD) for (uint_fast64_t p = 0; p < runsize / 2; p ++) swap(data[start + p], data[start + runsize - 1 - p]);
  buffer[start] = runsize;
  start = 0;
  for (uint_fast64_t current = 0; current < count; current += buffer[current] * ((buffer[current] < 0) ? -1 : 1))
    if (buffer[current] >= THRESHOLD) {
      if (start != current) quicksort_values(data + start, buffer[start] = current - start);
      start = current + buffer[current];
    }
  #undef THRESHOLD
  if (start != count) quicksort_values(data + start, buffer[start] = count - start);
  while (*buffer != count) {
    merge_sorted_values(data, count, buffer);
    merge_sorted_values(buffer, count, data);
  }
  free(buffer);
}

void quicksort_values (uint64_t * restrict data, uint64_t count) {
  switch (count) {
    case 3:
      if (*data > data[2]) swap(*data, data[2]);
      if (data[1] > data[2]) swap(data[1], data[2]);
    case 2:
      if (*data > data[1]) swap(*data, data[1]);
    case 0: case 1:
      return;
  }
  uint_fast64_t pivot = data[count / 2], left = SIZE_MAX, right = count;
  while (true) {
    while (data[++ left] < pivot);
    while (data[-- right] > pivot);
    if (left >= right) break;
    swap(data[left], data[right]);
  }
  right ++;
  if (right > 1) quicksort_values(data, right);
  if (count - right > 1) quicksort_values(data + right, count - right);
}

#undef swap

void merge_sorted_values (uint64_t * restrict data, uint64_t count, uint64_t * restrict runs) {
  // in: data = data to sort, runs = run lengths; out: flipped
  uint_fast64_t length;
  for (uint_fast64_t current = 0; current < count; current += length) {
    length = runs[current];
    if (current + length == count)
      memcpy(runs + current, data + current, length * sizeof *data);
    else {
      // index1, index2 point to the END of the respective runs
      uint_fast64_t remaining1 = length, index1 = current + remaining1, remaining2 = runs[index1], index2 = index1 + remaining2;
      length = remaining1 + remaining2;
      for (uint64_t p = 0; p < length; p ++)
        if (!remaining2 || (remaining1 && data[index1 - remaining1] <= data[index2 - remaining2]))
          runs[current + p] = data[index1 - (remaining1 --)];
        else
          runs[current + p] = data[index2 - (remaining2 --)];
    }
    data[current] = length;
  }
}
