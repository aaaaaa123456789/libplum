WARNFLAGS = -Wno-pointer-sign -Wno-dangling-else -Wno-parentheses -Wno-return-type
OPTFLAGS = -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x -Wl,--gc-sections -march=native -mtune=native

all: fuzz fuzz32 fuzztest fasttest

fuzz: fuzz.c libplum.c libplum.h
	clang-13 -O1 -g $(WARNFLAGS) -fsanitize=fuzzer,address fuzz.c libplum.c -o $@

fuzz32: fuzz.c libplum.c libplum.h
	clang-13 -O1 -g -m32 $(WARNFLAGS) -fsanitize=fuzzer,address fuzz.c libplum.c -o $@

fuzztest: fuzz.c libplum.c libplum.h fuzztest.c
	clang-13 -ggdb $(WARNFLAGS) -fsanitize=undefined fuzz.c libplum.c fuzztest.c -o $@

fasttest: fuzz.c libplum.c libplum.h fuzztest.c
	clang-13 $(OPTFLAGS) $(WARNFLAGS) -fsanitize=address,undefined fuzz.c libplum.c fuzztest.c -o $@

AFLCOMP = afl-clang-lto -g $(WARNFLAGS) -fsanitize=fuzzer fuzz.c libplum.c

afl: fuzz.c libplum.c libplum.h
	mkdir -p $@
	$(AFLCOMP) -o $@/fuzz
	AFL_LLVM_LAF_ALL=1 $(AFLCOMP) -o $@/fuzz-laf
	AFL_USE_ASAN=1 $(AFLCOMP) -o $@/fuzz-asan
	AFL_USE_UBSAN=1 $(AFLCOMP) -o $@/fuzz-ubsan
	AFL_LLVM_CMPLOG=1 $(AFLCOMP) -o $@/fuzz-cmplog

clean:
	rm -rf fuzz fuzztest fasttest afl
