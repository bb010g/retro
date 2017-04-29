#  ____   ____ ______ ____    ___
#  || \\ ||    | || | || \\  // \\
#  ||_// ||==    ||   ||_// ((   ))
#  || \\ ||___   ||   || \\  \\_//
#  a personal, minimalistic forth

CC = clang
LD = clang
LDFLAGS =
CFLAGS = -Wall -O3

all: clean sources tools compile link image rre finish opt

clean:
	rm -f bin/rre bin/nga bin/embedimage bin/extend bin/unu bin/muri

sources:
	cd source && $(CC) $(CFLAGS) unu.c -o ../bin/unu
	./bin/unu literate/Unu.md >source/unu.c
	./bin/unu literate/Nga.md >source/nga.c
	./bin/unu literate/Muri.md >source/muri.c
	./bin/unu literate/Rx.md >rx.muri
	./bin/unu literate/RetroForth.md >retro.forth

tools:
	cd source && $(CC) $(CFLAGS) unu.c -o ../bin/unu
	cd source && $(CC) $(CFLAGS) nga.c -DSTANDALONE -o ../bin/nga
	cd source && $(CC) $(CFLAGS) muri.c -o ../bin/muri

compile:
	cd source && $(CC) $(CFLAGS) -c nga.c -o nga.o
	cd source && $(CC) $(CFLAGS) -c extend.c -o extend.o
	cd source && $(CC) $(CFLAGS) -c embedimage.c -o embedimage.o
	cd source && $(CC) $(CFLAGS) -c rre.c -o rre.o
	mv source/*.o bin

link:
	cd bin && $(LD) $(LDFLAGS) nga.o extend.o -o extend
	cd bin && $(LD) $(LDFLAGS) embedimage.o -o embedimage

image:
	./bin/muri rx.muri
	./bin/extend retro.forth

rre:
	./bin/embedimage >source/image.c
	cd source && $(CC) $(CFLAGS) -c image.c -o ../bin/image.o
	cd bin && $(CC) $(CFLAGS) rre.o nga.o image.o -o rre

finish:
	rm -f bin/*.o

opt:
	cd optional && ../bin/unu literate/Array.md >array.forth
	cd optional && ../bin/unu literate/NS.md >ns.forth
