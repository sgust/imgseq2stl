# simple Makefile, all done by implicit rules

all: imgseq2stl

clean:
	rm *.o

distclean: clean
	rm imgseq2stl
