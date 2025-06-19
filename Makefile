CC = gcc
CFLAGS =
DESTDIR = /usr/local/bin
all: build/fbimg build/png2fbimg
	@echo "Build completed"
	@echo "Run make install to install to your system, or copy binaries from build/"

build/fbimg:
	@mkdir -p build
	$(CC) $(CFLAGS) fbimg.c -o build/fbimg

build/png2fbimg:
	@mkdir -p build
	$(CC) $(CFLAGS) thirdparty/lodepng/lodepng.c png2fbimg.c -o build/png2fbimg

clean:
	rm -rf build

install:
	cp build/* $(DESTDIR)
