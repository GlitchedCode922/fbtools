CC = gcc
CFLAGS =
DESTDIR = /usr/local

BINDIR = $(DESTDIR)/bin
LIBDIR = $(DESTDIR)/lib
HEADDIR = $(DESTDIR)/include

all: build/fbimg build/png2fbimg build/libscaleimg.a build/libscaleimg.so
	@echo "Build completed"
	@echo "Run make install to install to your system, or copy binaries from build/"

build/fbimg:
	@mkdir -p build
	$(CC) $(CFLAGS) scale_img.c fbimg.c -o build/fbimg

build/png2fbimg:
	@mkdir -p build
	$(CC) $(CFLAGS) thirdparty/lodepng/lodepng.c png2fbimg.c -o build/png2fbimg

build/libscaleimg.a:
	@mkdir -p build
	$(CC) -c scale_img.c -o build/scaleimg_a.o
	ar rcs build/libscaleimg.a build/scaleimg_a.o

build/libscaleimg.so:
	@mkdir -p build
	$(CC) -fPIC -c scale_img.c -o build/scaleimg_so.o
	$(CC) -shared build/scaleimg_so.o -o build/libscaleimg.so

clean:
	rm -rf build

install:
	@mkdir -p $(BINDIR)
	@mkdir -p $(LIBDIR)
	@mkdir -p $(HEADDIR)
	cp build/fbimg $(BINDIR)
	cp build/png2fbimg $(BINDIR)
	cp build/libscaleimg.a $(LIBDIR)
	cp build/libscaleimg.so $(LIBDIR)
	cp include/scale_img.h $(HEADDIR)
