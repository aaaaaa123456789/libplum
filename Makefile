CC ?= gcc
OUTPUT ?= libplum.so
OPTFLAGS = -march=native -mtune=native
DEBUGFLAGS =

CFLAGS = -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x -Wl,--gc-sections \
         -Wl,--no-eh-frame-hdr $(OPTFLAGS)

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
