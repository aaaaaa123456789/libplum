#include "proto.h"

uint64_t adjust_frame_duration (uint64_t duration, int64_t * restrict remainder) {
  if (*remainder < 0)
    if (duration < -*remainder) {
      *remainder += duration;
      return 0;
    } else {
      duration += (uint64_t) *remainder;
      *remainder = 0;
      return duration;
    }
  else {
    duration += *remainder;
    if (duration < *remainder) duration = UINT64_MAX;
    *remainder = 0;
    return duration;
  }
}

void update_frame_duration_remainder (uint64_t actual, uint64_t computed, int64_t * restrict remainder) {
  if (actual < computed) {
    uint64_t difference = computed - actual;
    if (difference > INT64_MAX) difference = INT64_MAX;
    if (*remainder >= 0 || difference - *remainder <= (uint64_t) INT64_MIN)
      *remainder -= (int64_t) difference;
    else
      *remainder = INT64_MIN;
  } else {
    uint64_t difference = actual - computed;
    if (difference > INT64_MAX) difference = INT64_MAX;
    if (*remainder < 0 || difference + *remainder <= (uint64_t) INT64_MAX)
      *remainder += (int64_t) difference;
    else
      *remainder = INT64_MAX;
  }
}

void calculate_frame_duration_fraction (uint64_t duration, uint32_t limit, uint32_t * restrict numerator, uint32_t * restrict denominator) {
  // if the number is too big to be represented at all, just fail early and return infinity
  if (duration >= 1000000000u * ((uint64_t) limit + 1)) {
    *numerator = 1;
    *denominator = 0;
    return;
  }
  // if the number can be represented exactly, do that
  *denominator = 1000000000u;
  while (!((duration | *denominator) & 1)) {
    duration >>= 1;
    *denominator >>= 1;
  }
  while (!(duration % 5 || *denominator % 5)) {
    duration /= 5;
    *denominator /= 5;
  }
  if (duration <= limit && *denominator <= limit) {
    *numerator = duration;
    return;
  }
  // otherwise, calculate the coefficients of the value's continued fraction representation until the represented fraction exceeds the range limit
  // this will necessarily stop before running out of coefficients because we know at this stage that the exact value doesn't fit
  uint64_t cumulative_numerator = duration / *denominator, cumulative_denominator = 1, previous_numerator = 1, previous_denominator = 0;
  uint32_t coefficient, original_denominator = *denominator;
  *numerator = duration % *denominator;
  while (true) {
    coefficient = *denominator / *numerator;
    uint64_t partial_numerator = *denominator % *numerator;
    *denominator = *numerator;
    *numerator = partial_numerator;
    if (cumulative_numerator > cumulative_denominator) {
      uint64_t partial_cumulative_numerator = cumulative_numerator * coefficient + previous_numerator;
      if (partial_cumulative_numerator > limit) break;
      previous_numerator = cumulative_numerator;
      cumulative_numerator = partial_cumulative_numerator;
      uint64_t partial_cumulative_denominator = cumulative_denominator * coefficient + previous_denominator;
      previous_denominator = cumulative_denominator;
      cumulative_denominator = partial_cumulative_denominator;
    } else {
      uint64_t partial_cumulative_denominator = cumulative_denominator * coefficient + previous_denominator;
      if (partial_cumulative_denominator > limit) break;
      previous_denominator = cumulative_denominator;
      cumulative_denominator = partial_cumulative_denominator;
      uint64_t partial_cumulative_numerator = cumulative_numerator * coefficient + previous_numerator;
      previous_numerator = cumulative_numerator;
      cumulative_numerator = partial_cumulative_numerator;
    }
  }
  // the current coefficient would be too large to fit, so try reducing it until it fits; if a good coefficient is found, use it
  uint64_t threshold = coefficient / 2 + 1;
  if (cumulative_numerator > cumulative_denominator) {
    while (-- coefficient >= threshold) if (cumulative_numerator * coefficient + previous_numerator <= limit) break;
  } else
    while (-- coefficient >= threshold) if (cumulative_denominator * coefficient + previous_denominator <= limit) break;
  if (coefficient >= threshold) {
    *numerator = cumulative_numerator * coefficient + previous_numerator;
    *denominator = cumulative_denominator * coefficient + previous_denominator;
    return;
  }
  // reducing the coefficient to half its original value may or may not improve accuracy, so this must be tested
  // if it doesn't, return the previous step's values; if it does, return the improved values
  *numerator = cumulative_numerator;
  *denominator = cumulative_denominator;
  if (coefficient) {
    cumulative_numerator = cumulative_numerator * coefficient + previous_numerator;
    cumulative_denominator = cumulative_denominator * coefficient + previous_denominator;
    if (cumulative_numerator > limit || cumulative_denominator > limit) return;
    /* The exact value, old approximation and new approximation are respectively proportional to the products of three quantities:
       exact value       ~ *denominator * duration * cumulative_denominator
       old approximation ~ *numerator * original_denominator * cumulative_denominator
       new approximation ~ *denominator * original_denominator * cumulative_numerator
       The problem is that these quantities are 96 bits wide, and thus some care must be exercised when computing them and comparing them. */
    uint32_t exact_low, old_low, new_low; // bits 0-31
    uint64_t exact_high, old_high, new_high; // bits 32-95
    uint64_t partial_exact = *denominator * cumulative_denominator;
    exact_high = (partial_exact >> 32) * duration + (duration >> 32) * partial_exact;
    partial_exact = (partial_exact & 0xffffffffu) * (duration & 0xffffffffu);
    exact_high += partial_exact >> 32;
    exact_low = partial_exact;
    uint64_t partial_old = *numerator * (uint64_t) original_denominator;
    old_high = (partial_old >> 32) * cumulative_denominator;
    partial_old = (partial_old & 0xffffffffu) * cumulative_denominator;
    old_high += partial_old >> 32;
    old_low = partial_old;
    uint64_t partial_new = *denominator * (uint64_t) original_denominator;
    new_high = (partial_new >> 32) * cumulative_numerator;
    partial_new = (partial_new & 0xffffffffu) * cumulative_numerator;
    new_high += partial_new >> 32;
    new_low = partial_new;
    // do the subtractions, and abuse two's complement to deal with negative results
    old_high -= exact_high;
    uint64_t old_diff = (uint64_t) old_low - exact_low;
    old_low = old_diff;
    if (old_diff & 0xffffffff00000000u) old_high --;
    if (old_high & 0x8000000000000000u)
      if (old_low) {
        old_high = ~old_high;
        old_low = -old_low;
      } else
        old_high = -old_high;
    new_high -= exact_high;
    uint64_t new_diff = (uint64_t) new_low - exact_low;
    new_low = new_diff;
    if (new_diff & 0xffffffff00000000u) new_high --;
    if (new_high & 0x8000000000000000u)
      if (new_low) {
        new_high = ~new_high;
        new_low = -new_low;
      } else
        new_high = -new_high;
    // and finally, compare and use the new results if they are better
    if (new_high < old_high || (new_high == old_high && new_low <= old_low)) {
      *numerator = cumulative_numerator;
      *denominator = cumulative_denominator;
    }
  }
}
