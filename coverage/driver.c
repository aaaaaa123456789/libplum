// This section has a single purpose: include code from other programs, presumably stand-alone ones, while replacing their main functions with a different name.
// That way, they can be compiled all at once into a single big program that can be instrumented for coverage.
// The fuzzer code itself is also included this way, so a single object file is needed for all the non-instrumented code.

#include "../fuzz.c"

#define main testopen
#include "testopen.c"
#undef main

#define main fuzztest
#include "../fuzztest.c"
#undef main

#define main strtest
#include "strtest.c"
#undef main

const struct module {
  const char * name;
  int (* callback) (int, char **);
} modules[] = {
  "check",   &fuzztest,
  "strings", &strtest,
  "test",    &testopen,
  NULL,      NULL
};

// With all modules properly included, actually invoke the correct module from the true main function

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../libplum.h"

void show_version(unsigned long);

int main (int argc, char ** argv) {
  uint32_t version = plum_get_version_number();
  if (version != PLUM_VERSION) {
    fprintf(stderr, "critical: library version mismatch (expected %u, got %u)\n", (unsigned) PLUM_VERSION, (unsigned) version);
    abort();
  }
  if (argc < 2) {
    show_version(version);
    return 0;
  }
  const char * modulename = argv[1];
  argc --;
  argv ++;
  const struct module * module;
  for (module = modules; module -> name; module ++) if (!strcmp(modulename, module -> name)) return module -> callback(argc, argv);
  fprintf(stderr, "error: unknown module %s\n", modulename);
  return 1;
}

void show_version (unsigned long version) {
  printf("libplum coverage test - library version %lu.%lu", version / 10000, (version % 10000) / 10);
  version %= 10;
  switch (version) {
    case 0:
      puts(" (development version)");
      break;
    case 9:
      putchar('\n');
      break;
    default:
      printf(" (release candidate %lu)\n", version);
  }
}
