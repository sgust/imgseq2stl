# simple Makefile, all done by implicit rules

all: imgseq2stl

clean:
	rm -f *.o

distclean: clean
	rm -f imgseq2stl
