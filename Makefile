# simple Makefile, all done by implicit rules

CFLAGS+=-Wall -O2 -g `pkg-config vips --cflags --libs` -lbsd

all: imgseq2stl

clean:
	rm -f *.o

distclean: clean
	rm -f imgseq2stl
