#include "proto.h"

struct JPEG_encoded_value * generate_JPEG_luminance_data_stream (struct context * context, double (* restrict data)[64], size_t units,
                                                                 const uint8_t quantization[restrict static 64], size_t * restrict count) {
  struct JPEG_encoded_value * result = NULL;
  *count = 0;
  size_t unit;
  double predicted = 0.0;
  for (unit = 0; unit < units; unit ++) predicted = generate_JPEG_data_unit(context, &result, count, data[unit], quantization, predicted);
  return result;
}

struct JPEG_encoded_value * generate_JPEG_chrominance_data_stream (struct context * context, double (* restrict blue)[64], double (* restrict red)[64],
                                                                   size_t units, const uint8_t quantization[restrict static 64], size_t * restrict count) {
  struct JPEG_encoded_value * result = NULL;
  *count = 0;
  size_t unit;
  double predicted_blue = 0.0, predicted_red = 0.0;
  for (unit = 0; unit < units; unit ++) {
    predicted_blue = generate_JPEG_data_unit(context, &result, count, blue[unit], quantization, predicted_blue);
    predicted_red = generate_JPEG_data_unit(context, &result, count, red[unit], quantization, predicted_red);
  }
  return result;
}

double generate_JPEG_data_unit (struct context * context, struct JPEG_encoded_value ** data, size_t * restrict count, const double unit[restrict static 64],
                                const uint8_t quantization[restrict static 64], double predicted) {
  int16_t output[64];
  predicted = apply_JPEG_DCT(output, unit, quantization, predicted);
  *data = ctxrealloc(context, *data, (*count + 64) * sizeof **data);
  size_t p, last = 0;
  encode_JPEG_value(*data + ((*count) ++), *output, 0, 0);
  for (p = 1; p < 63; p ++) if (output[p]) {
    for (; (p - last) > 16; last += 16) (*data)[(*count) ++] = (struct JPEG_encoded_value) {.code = 0xf0, .bits = 0, .type = 1};
    encode_JPEG_value(*data + ((*count) ++), output[p], 1, (p - last - 1) << 4);
    last = p;
  }
  if (last != 63) (*data)[(*count) ++] = (struct JPEG_encoded_value) {.code = 0, .bits = 0, .type = 1};
  return predicted;
}

void encode_JPEG_value (struct JPEG_encoded_value * data, int16_t value, unsigned type, unsigned char addend) {
  unsigned bits = bit_width(absolute_value(value));
  if (value < 0) value += 0x7fff; // make it positive and subtract 1 from the significant bits
  value &= (1 << bits) - 1;
  *data = (struct JPEG_encoded_value) {.code = addend + bits, .bits = bits, .type = type, .value = value};
}

size_t generate_JPEG_Huffman_table (struct context * context, const struct JPEG_encoded_value * data, size_t count, unsigned char * restrict output,
                                    unsigned char table[restrict static 0x100], unsigned char index) {
  // returns the number of bytes spent encoding the table in the JPEG data (in output)
  size_t counts[0x101] = {[0x100] = 1}; // use 0x100 as a dummy value to absorb the highest (invalid) code
  unsigned char lengths[0x101];
  size_t p;
  *output = index;
  index >>= 4;
  for (p = 0; p < count; p ++) if (data[p].type == index) counts[data[p].code] ++;
  generate_Huffman_tree(context, counts, lengths, 0x101, 16);
  unsigned char codecounts[16] = {0};
  unsigned char maxcode, maxlength = 0;
  for (p = 0; p < 0x100; p ++) if (lengths[p]) {
    codecounts[lengths[p] - 1] ++;
    if (lengths[p] > maxlength) {
      maxlength = lengths[p];
      maxcode = p;
    }
  }
  if (lengths[0x100] < maxlength) {
    codecounts[maxlength] --;
    codecounts[lengths[0x100]] ++;
    lengths[maxcode] = lengths[0x100];
  }
  memcpy(table, lengths, 0x100);
  memcpy(output + 1, codecounts, 16);
  size_t outsize = 17;
  for (maxlength = 1; maxlength <= 16; maxlength ++) for (p = 0; p < 0x100; p ++) if (lengths[p] == maxlength) output[outsize ++] = p;
  return outsize;
}

void encode_JPEG_scan (struct context * context, const struct JPEG_encoded_value * data, size_t count, const unsigned char table[restrict static 0x200]) {
  unsigned short codes[0x200]; // no need to create a dummy entry for the highest (invalid) code here: it simply won't be generated
  generate_Huffman_codes(codes, 0x100, table, 0);
  generate_Huffman_codes(codes + 0x100, 0x100, table + 0x100, 0);
  unsigned char * node = append_output_node(context, 0x4000);
  size_t p, size = 0;
  uint_fast32_t output = 0;
  unsigned char bits = 0;
  for (p = 0; p < count; p ++) {
    if (size > 0x3ff8) {
      context -> output -> size = size;
      node = append_output_node(context, 0x4000);
      size = 0;
    }
    unsigned short index = data[p].type * 0x100 + data[p].code;
    output = (output << table[index]) | codes[index];
    bits += table[index];
    while (bits >= 8) {
      node[size ++] = output >> (bits -= 8);
      if (node[size - 1] == 0xff) node[size ++] = 0;
    }
    if (data[p].bits) {
      output = (output << data[p].bits) | data[p].value;
      bits += data[p].bits;
      while (bits >= 8) {
        node[size ++] = output >> (bits -= 8);
        if (node[size - 1] == 0xff) node[size ++] = 0;
      }
    }
  }
  if (bits) node[size ++] = output << (8 - bits);
  context -> output -> size = size;
}
