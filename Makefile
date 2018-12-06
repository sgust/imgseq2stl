# simple Makefile, all done by implicit rules

CFLAGS+=-Wall -O2

all: imgseq2stl

clean:
	rm -f *.o

distclean: clean
	rm -f imgseq2stl
