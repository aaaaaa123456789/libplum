CC ?= gcc
OUTPUT ?= libplum.so
CFLAGS = -Ofast -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-exceptions -Wl,-S -Wl,-x -Wl,--gc-sections \
         -Wl,--no-eh-frame-hdr -march=native -mtune=native

all: build/libplum.c build/libplum.h
	$(CC) -shared $(CFLAGS) $^ -o build/$(OUTPUT)

clean:
	rm -rf build

build/libplum.c: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh $(wildcard src/*.c) > $@

debug: build/libplum-debug.so build/libplum.h

build/libplum-debug.so: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h)
	mkdir -p build
	$(CC) -shared -ggdb -fPIC -DPLUM_DEBUG $(wildcard src/*.c) -o $@

build/libplum.h: $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh header/libplum.h > $@
