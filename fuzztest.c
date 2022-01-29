#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>

int LLVMFuzzerTestOneInput(const unsigned char *, size_t);

int main (int argc, char ** argv) {
  while (*++ argv) {
    FILE * fp = fopen(*argv, "rb");
    if (!fp) continue;
    char * data = NULL;
    size_t size = 0;
    while (!feof(fp)) {
      data = realloc(data, size + 0x4000);
      size += fread(data + size, 1, 0x4000, fp);
    }
    fclose(fp);
    if (argc > 2) {
      struct tm * timedata = localtime((const time_t []) {time(NULL)});
      printf("[%02d:%02d:%02d] %s\n", timedata -> tm_hour, timedata -> tm_min, timedata -> tm_sec,  *argv);
    }
    LLVMFuzzerTestOneInput(data, size);
    free(data);
  }
  return 0;
}
