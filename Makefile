PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/RETRO12
DOCSDIR ?= $(PREFIX)/share/doc/RETRO12
EXAMPLESDIR ?= $(PREFIX)/share/examples/RETRO12
MANDIR ?= $(PREFIX)/man/man1
LIBM ?= -lm
LIBCURSES ?= -lcurses

RREIO ?= io/filesystem.o io/floatingpoint.o io/gopher.o io/unix.o
RIIO ?= io/filesystem.o io/floatingpoint.o io/gopher.o io/unix.o

all: build

help:

build: dirs toolchain ngaImage bin/retro bin/retro-describe

optional: build bin/retro-repl

toolchain: dirs bin/retro-embedimage bin/retro-extend bin/retro-muri bin/retro-unu

dirs:
	mkdir -p bin

clean:
	rm -f bin/*
	find . -name "*.o" -type f -delete

install: build install-data install-docs install-examples install-manpages
	install -m 755 -d -- $(DESTDIR)$(PREFIX)/bin
	install -c -m 755 bin/retro-embedimage $(DESTDIR)$(PREFIX)/bin/retro-embedimage
	install -c -m 755 bin/retro-extend $(DESTDIR)$(PREFIX)/bin/retro-extend
	install -c -m 755 bin/retro-muri $(DESTDIR)$(PREFIX)/bin/retro-muri
	install -c -m 755 bin/retro $(DESTDIR)$(PREFIX)/bin/retro
	install -c -m 755 bin/retro-unu $(DESTDIR)$(PREFIX)/bin/retro-unu
	install -c -m 755 bin/retro-describe $(DESTDIR)$(PREFIX)/bin/retro-describe

install-strip: build install-data install-docs install-examples
	install -m 755 -d -- $(DESTDIR)/bin
	install -c -m 755 -s bin/retro-embedimage $(DESTDIR)$(PREFIX)/bin/retro-embedimage
	install -c -m 755 -s bin/retro-extend $(DESTDIR)$(PREFIX)/bin/retro-extend
	install -c -m 755 -s bin/retro-muri $(DESTDIR)$(PREFIX)/bin/retro-muri
	install -c -m 755 -s bin/retro $(DESTDIR)$(PREFIX)/bin/retro
	install -c -m 755 -s bin/retro-unu $(DESTDIR)$(PREFIX)/bin/retro-unu

install-data:
	install -m 755 -d -- $(DESTDIR)$(DATADIR)
	install -c -m 644 glossary.forth $(DESTDIR)$(DATADIR)/glossary.forth
	install -c -m 644 ngaImage $(DESTDIR)$(DATADIR)/ngaImage
	cp -fpR tests $(DESTDIR)$(DATADIR)/
	install -c -m 644 words.tsv $(DESTDIR)$(DATADIR)/words.tsv

install-docs:
	install -m 755 -d -- $(DESTDIR)$(DOCSDIR)
	cp -fpR doc $(DESTDIR)$(DOCSDIR)
	cp -fpR literate $(DESTDIR)$(DOCSDIR)
	install -c -m 644 README.md $(DESTDIR)$(DOCSDIR)/README.md
	install -c -m 644 RELEASE_NOTES.md $(DESTDIR)$(DOCSDIR)/RELEASE_NOTES.md

install-examples:
	install -m 755 -d -- $(DESTDIR)$(EXAMPLESDIR)
	cp -fpR example $(DESTDIR)$(EXAMPLESDIR)

install-manpages:
	install -c -m 644 man/retro.1 $(MANDIR)/retro.1
	install -c -m 644 man/retro-embedimage.1 $(MANDIR)/retro-embedimage.1
	install -c -m 644 man/retro-extend.1 $(MANDIR)/retro-extend.1
	install -c -m 644 man/retro-describe.1 $(MANDIR)/retro-describe.1
	install -c -m 644 man/retro-muri.1 $(MANDIR)/retro-muri.1
	install -c -m 644 man/retro-unu.1 $(MANDIR)/retro-unu.1


test: bin/retro
	./bin/retro tests/test-core.forth

# Targets for development/interactive usage

glossary: doc/Glossary.txt doc/Glossary.html doc/Glossary-Concise.txt doc/Glossary-Names-and-Stack.txt

image: interfaces/image.c

js: bin/RETRO12.html

repl: bin/retro-repl

retro-describe: bin/retro-describe

update: bin/retro-unu literate/Unu.md literate/Muri.md
	./bin/retro-unu literate/Unu.md >tools/unu.c
	./bin/retro-unu literate/Muri.md >tools/muri.c

# File targets.

ngaImage: literate/Rx.md literate/RetroForth.md bin/retro-muri bin/retro-extend
	./bin/retro-muri literate/Rx.md
	./bin/retro-extend ngaImage literate/RetroForth.md

bin/retro-describe: retro-describe.forth words.tsv
	cat retro-describe.forth words.tsv >bin/retro-describe

bin/retro-embedimage: tools/embedimage.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-embedimage  tools/embedimage.c

bin/retro-extend: tools/extend.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-extend  tools/extend.c

bin/retro-injectimage-js: tools/injectimage-js.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-injectimage-js  tools/injectimage-js.c

bin/retro-muri: tools/muri.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-muri tools/muri.c

bin/RETRO12.html: bin/retro-injectimage-js
	./bin/retro-injectimage-js >bin/RETRO12.html

bin/retro-repl: interfaces/repl.c interfaces/image.c
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro-repl repl.c

bin/retro: io bin/retro-embedimage bin/retro-extend interfaces/image.c interfaces/rre.c interfaces/rre.forth interfaces/image-functions.o interfaces/io/rng.forth
	cp ngaImage rre.image
	./bin/retro-extend rre.image interfaces/io/filesystem.forth interfaces/io/gopher.forth interfaces/io/floatingpoint.forth interfaces/io/unix.forth interfaces/io/rng.forth interfaces/rre.forth
	./bin/retro-embedimage rre.image >interfaces/rre_image.c
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro $(LIBM) rre.c image-functions.o $(RREIO)
	cd package && ../bin/retro -f list
	./bin/retro-embedimage rre.image >interfaces/rre_image.c
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro $(LIBM) rre.c image-functions.o $(RREIO)
	rm rre.image

bin/retro-barebones: bin/retro-embedimage bin/retro-extend interfaces/image.c interfaces/barebones.c interfaces/barebones.forth
	cp ngaImage barebones.image
	./bin/retro-extend barebones.image interfaces/barebones.forth
	./bin/retro-embedimage barebones.image >interfaces/barebones_image.c
	rm barebones.image
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro-barebones barebones.c

bin/retro-unu: tools/unu.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-unu tools/unu.c

doc/Glossary.txt: bin/retro words.tsv
	LC_ALL=C sort -o sorted.tsv words.tsv
	mv sorted.tsv words.tsv
	./bin/retro glossary.forth export glossary >doc/Glossary.txt

doc/Glossary.html: bin/retro words.tsv
	LC_ALL=C sort -o sorted.tsv words.tsv
	mv sorted.tsv words.tsv
	./bin/retro glossary.forth export html >doc/Glossary.html

doc/Glossary-Concise.txt: bin/retro words.tsv
	LC_ALL=C sort -o sorted.tsv words.tsv
	mv sorted.tsv words.tsv
	./bin/retro glossary.forth export concise >doc/Glossary-Concise.txt

doc/Glossary-Names-and-Stack.txt: bin/retro words.tsv
	LC_ALL=C sort -o sorted.tsv words.tsv
	mv sorted.tsv words.tsv
	./bin/retro glossary.forth export concise-stack >doc/Glossary-Names-and-Stack.txt


interfaces/image.c: bin/retro-embedimage bin/retro-extend bin/retro-muri literate/RetroForth.md literate/Rx.md
	./bin/retro-muri literate/Rx.md
	./bin/retro-extend ngaImage literate/RetroForth.md
	./bin/retro-embedimage ngaImage > interfaces/image.c

bin/retro-compiler: io bin/retro-extend interfaces/image.c interfaces/retro-compiler.c interfaces/retro-compiler-runtime.c interfaces/image-functions.o
	cp ngaImage runtime.image
	./bin/retro-extend runtime.image interfaces/io/filesystem.forth interfaces/io/gopher.forth interfaces/io/floatingpoint.forth interfaces/io/unix.forth interfaces/io/rng.forth interfaces/rre.forth
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../retro-compiler-runtime $(LIBM) retro-compiler-runtime.c image-functions.c $(RREIO)
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro-compiler retro-compiler.c
	objcopy --add-section .ngaImage=runtime.image --set-section-flags .ngaImage=noload,readonly bin/retro-compiler
	objcopy --add-section .runtime=retro-compiler-runtime --set-section-flags .runtime=noload,readonly bin/retro-compiler
	rm runtime.image retro-compiler-runtime

io: interfaces/io/filesystem.o interfaces/io/floatingpoint.o interfaces/io/gopher.o interfaces/io/unix.o

interfaces/image-functions.o: interfaces/image-functions.c interfaces/image-functions.h
	cd interfaces && $(CC) $(CFLAGS) -c -o image-functions.o image-functions.c

interfaces/io/filesystem.o: interfaces/io/filesystem.forth interfaces/io/filesystem.c
	cd interfaces/io && $(CC) $(CFLAGS) -c -o filesystem.o filesystem.c

interfaces/io/floatingpoint.o: interfaces/io/floatingpoint.forth interfaces/io/floatingpoint.c
	cd interfaces/io && $(CC) $(CFLAGS) -c -o floatingpoint.o floatingpoint.c

interfaces/io/gopher.o: interfaces/io/gopher.forth interfaces/io/gopher.c
	cd interfaces/io && $(CC) $(CFLAGS) -c -o gopher.o gopher.c

interfaces/io/unix.o: interfaces/io/unix.forth interfaces/io/unix.c
	cd interfaces/io && $(CC) $(CFLAGS) -c -o unix.o unix.c
