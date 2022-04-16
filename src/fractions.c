#include "proto.h"

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
  uint64_t temp, cumulative_numerator = duration / *denominator, cumulative_denominator = 1, previous_numerator = 1, previous_denominator = 0;
  uint32_t coefficient, original_denominator = *denominator;
  *numerator = duration % *denominator;
  while (1) {
    coefficient = *denominator / *numerator;
    temp = *denominator % *numerator;
    *denominator = *numerator;
    *numerator = temp;
    if (cumulative_numerator > cumulative_denominator) {
      temp = cumulative_numerator * coefficient + previous_numerator;
      if (temp > limit) break;
      previous_numerator = cumulative_numerator;
      cumulative_numerator = temp;
      temp = cumulative_denominator * coefficient + previous_denominator;
      previous_denominator = cumulative_denominator;
      cumulative_denominator = temp;
    } else {
      temp = cumulative_denominator * coefficient + previous_denominator;
      if (temp > limit) break;
      previous_denominator = cumulative_denominator;
      cumulative_denominator = temp;
      temp = cumulative_numerator * coefficient + previous_numerator;
      previous_numerator = cumulative_numerator;
      cumulative_numerator = temp;
    }
  }
  // the current coefficient would be too large to fit, so try reducing it until it fits; if a good coefficient is found, use it
  temp = coefficient / 2 + 1;
  if (cumulative_numerator > cumulative_denominator) {
    while (-- coefficient >= temp) if (cumulative_numerator * coefficient + previous_numerator <= limit) break;
  } else
    while (-- coefficient >= temp) if (cumulative_denominator * coefficient + previous_denominator <= limit) break;
  if (coefficient >= temp) {
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
    temp = *denominator * cumulative_denominator;
    exact_high = (temp >> 32) * duration + (duration >> 32) * temp;
    temp = (temp & 0xffffffffu) * (duration & 0xffffffffu);
    exact_high += temp >> 32;
    exact_low = temp;
    temp = *numerator * (uint64_t) original_denominator;
    old_high = (temp >> 32) * cumulative_denominator;
    temp = (temp & 0xffffffffu) * cumulative_denominator;
    old_high += temp >> 32;
    old_low = temp;
    temp = *denominator * (uint64_t) original_denominator;
    new_high = (temp >> 32) * cumulative_numerator;
    temp = (temp & 0xffffffffu) * cumulative_numerator;
    new_high += temp >> 32;
    new_low = temp;
    // do the subtractions, and abuse two's complement to deal with negative results
    old_high -= exact_high;
    temp = (uint64_t) old_low - exact_low;
    old_low = temp;
    if (temp & 0xffffffff00000000u) old_high --;
    if (old_high & 0x8000000000000000u)
      if (old_low) {
        old_high = ~old_high;
        old_low = -old_low;
      } else
        old_high = -old_high;
    new_high -= exact_high;
    temp = (uint64_t) new_low - exact_low;
    new_low = temp;
    if (temp & 0xffffffff00000000u) new_high --;
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
