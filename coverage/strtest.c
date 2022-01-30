#include <stdio.h>
#include <stdlib.h>
#include "../libplum.h"

int main (int argc, char ** argv) {
  while (*++ argv) {
    if (!**argv) continue;
    char * error;
    unsigned long num = strtoul(*argv, &error, 0);
    if (*error || (num > 0xffffu)) {
      fprintf(stderr, "error: invalid number: %s\n", *argv);
      return 1;
    }
    const char * name = plum_get_file_format_name(num);
    if (name) printf("format %5lu: %s\n", num, name);
    name = plum_get_error_text(num);
    if (name) printf("error %6lu: %s\n", num, name);
  }
  return 0;
}
