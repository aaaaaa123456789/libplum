CLANG ?= clang
WARNFLAGS = -Wno-pointer-sign -Wno-dangling-else -Wno-parentheses -Wno-return-type
OPTFLAGS = -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x -Wl,--gc-sections -march=native -mtune=native

BINARIES = fuzz fuzz32 fuzztest fasttest

all: $(BINARIES)

fuzz: fuzz.c libplum.c libplum.h
	$(CLANG) -O1 -g $(WARNFLAGS) -fsanitize=fuzzer,address fuzz.c libplum.c -o $@

fuzz32: fuzz.c libplum.c libplum.h
	$(CLANG) -O1 -g -m32 $(WARNFLAGS) -fsanitize=fuzzer,address fuzz.c libplum.c -o $@

fuzztest: fuzz.c libplum.c libplum.h fuzztest.c
	$(CLANG) -ggdb $(WARNFLAGS) -fsanitize=undefined fuzz.c libplum.c fuzztest.c -o $@

fasttest: fuzz.c libplum.c libplum.h fuzztest.c
	$(CLANG) $(OPTFLAGS) $(WARNFLAGS) -fsanitize=address,undefined fuzz.c libplum.c fuzztest.c -o $@

coverage/driver.o: libplum.h fuzz.c fuzztest.c $(wildcard coverage/*.c)
	$(CLANG) -c -O1 -g $(WARNFLAGS) coverage/driver.c
	mv driver.o coverage/

covtest: libplum.c libplum.h coverage/driver.o
	$(CLANG) -O1 -g $(WARNFLAGS) -fprofile-instr-generate -fcoverage-mapping coverage/driver.o libplum.c -o $@

libtest: libplum.c libplum.h coverage/driver.o
	$(CLANG) -O1 -g $(WARNFLAGS) coverage/driver.o libplum.c -o $@

AFLCC ?= afl-clang-lto
AFLCOMP = $(AFLCC) -g $(WARNFLAGS) -fsanitize=fuzzer fuzz.c libplum.c

afl: fuzz.c libplum.c libplum.h
	mkdir -p $@
	$(AFLCOMP) -o $@/fuzz
	AFL_LLVM_LAF_ALL=1 $(AFLCOMP) -o $@/fuzz-laf
	AFL_USE_ASAN=1 $(AFLCOMP) -o $@/fuzz-asan
	AFL_USE_UBSAN=1 $(AFLCOMP) -o $@/fuzz-ubsan
	AFL_LLVM_CMPLOG=1 $(AFLCOMP) -o $@/fuzz-cmplog

clean:
	rm -rf $(BINARIES) covtest libtest afl
