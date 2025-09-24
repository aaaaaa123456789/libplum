HAVE_UNAME := $(shell command -v uname >/dev/null 2>&1 && echo 1)
UNAME_S := $(if $(HAVE_UNAME),$(shell uname -s),)

CC ?= gcc
OUTPUT ?= libplum.so
OPTFLAGS = -march=native -mtune=native

DEFAULTCFLAGS := -std=c17 -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x
ifeq ($(UNAME_S),Darwin)
	CFLAGS = $(DEFAULTCFLAGS) $(OPTFLAGS)
else
	CFLAGS = $(DEFAULTCFLAGS) -Wl,--gc-sections $(OPTFLAGS)
endif

DEBUGFLAGS = -Wall -Wextra -pedantic -Wnull-dereference -Wshadow -Wundef -Wunused -Wwrite-strings -Wno-sign-compare -Wno-implicit-fallthrough \
             -Wno-parentheses -Wno-dangling-else -Wno-type-limits

ifneq (,$(findstring gcc,$(CC)))
	DEBUGFLAGS += -Wduplicated-branches -Wduplicated-cond -Wlogical-op -Wshift-overflow=2 -fanalyzer -fanalyzer-verbosity=0 \
	              -Wno-analyzer-use-of-uninitialized-value
else
ifneq (,$(findstring clang, $(CC)))
	DEBUGFLAGS += -Wno-keyword-macro -fsanitize=undefined -Wno-tautological-constant-out-of-range-compare
endif
endif

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
