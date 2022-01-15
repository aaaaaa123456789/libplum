CC ?= gcc
OUTPUT ?= libplum.so
OPTFLAGS = -march=native -mtune=native

CFLAGS = -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x -Wl,--gc-sections \
         -std=c17 $(OPTFLAGS)

DEBUGFLAGS = -Wall -Wextra -pedantic -Wcast-align -Wduplicated-branches -Wduplicated-cond -Wlogical-op \
             -Wnull-dereference -Wshadow -Wshift-overflow=2 -Wundef -Wunused -Wwrite-strings -Wno-sign-compare \
             -Wno-implicit-fallthrough -Wno-parentheses -Wno-dangling-else -fanalyzer -fanalyzer-verbosity=0

.PHONY: all clean basefiles debug

all: basefiles
	$(CC) -shared -fPIC $(CFLAGS) build/libplum.c -o build/$(OUTPUT)

clean:
	rm -rf build

basefiles: build/libplum.c build/libplum.h

debug: build/libplum-debug.so build/libplum.h

build/libplum.c: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh $(sort $(wildcard src/*.c)) > $@

build/libplum-debug.so: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h)
	mkdir -p build
	$(CC) -shared -ggdb -fPIC -DPLUM_DEBUG $(DEBUGFLAGS) $(wildcard src/*.c) -o $@

build/libplum.h: $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh header/libplum.h > $@
