PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/RETRO12
DOCSDIR ?= $(PREFIX)/share/doc/RETRO12
EXAMPLESDIR ?= $(PREFIX)/share/examples/RETRO12
LIBM ?= -lm
LIBCURSES ?= -lcurses

all: build

build: dirs bin/retro-embedimage bin/retro-extend bin/retro-injectimage-js bin/retro-muri bin/RETRO12.html bin/retro-ri bin/retro bin/retro-repl bin/retro-unu

dirs:
	mkdir -p bin

clean:
	rm -f bin/retro-embedimage
	rm -f bin/retro-extend
	rm -f bin/retro-injectimage-js
	rm -f bin/retro-muri
	rm -f bin/RETRO12.html
	rm -f bin/retro-repl
	rm -f bin/retro-ri
	rm -f bin/retro
	rm -f bin/retro-unu

install: build install-data install-docs install-examples
	install -m 755 -d -- $(DESTDIR)$(PREFIX)/bin
	install -c -m 755 bin/retro-embedimage $(DESTDIR)$(PREFIX)/bin/retro-embedimage
	install -c -m 755 bin/retro-extend $(DESTDIR)$(PREFIX)/bin/retro-extend
	install -c -m 755 bin/retro-injectimage-js $(DESTDIR)$(PREFIX)/bin/retro-injectimage-js
	install -c -m 755 bin/retro-listener $(DESTDIR)$(PREFIX)/bin/retro-listener
	install -c -m 755 bin/retro-muri $(DESTDIR)$(PREFIX)/bin/retro-muri
	install -c -m 755 bin/retro-repl $(DESTDIR)$(PREFIX)/bin/retro-repl
	install -c -m 755 bin/retro-ri $(DESTDIR)$(PREFIX)/bin/retro-ri
	install -c -m 755 bin/retro $(DESTDIR)$(PREFIX)/bin/retro
	install -c -m 755 bin/retro-unu $(DESTDIR)$(PREFIX)/bin/retro-unu

install-strip: build install-data install-docs install-examples
	install -m 755 -d -- $(DESTDIR)/bin
	install -c -m 755 -s bin/retro-embedimage $(DESTDIR)$(PREFIX)/bin/retro-embedimage
	install -c -m 755 -s bin/retro-extend $(DESTDIR)$(PREFIX)/bin/retro-extend
	install -c -m 755 -s bin/retro-injectimage-js $(DESTDIR)$(PREFIX)/bin/retro-injectimage-js
	install -c -m 755 bin/retro-listener $(DESTDIR)$(PREFIX)/bin/retro-listener
	install -c -m 755 -s bin/retro-muri $(DESTDIR)$(PREFIX)/bin/retro-muri
	install -c -m 755 -s bin/retro-repl $(DESTDIR)$(PREFIX)/bin/retro-repl
	install -c -m 755 -s bin/retro-ri $(DESTDIR)$(PREFIX)/bin/retro-ri
	install -c -m 755 -s bin/retro $(DESTDIR)$(PREFIX)/bin/retro
	install -c -m 755 -s bin/retro-unu $(DESTDIR)$(PREFIX)/bin/retro-unu

install-data: bin/RETRO12.html
	install -m 755 -d -- $(DESTDIR)$(DATADIR)
	install -c -m 644 bin/RETRO12.html $(DESTDIR)$(DATADIR)/RETRO12.html
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

test: bin/retro
	./bin/retro tests/test-core.forth

# Targets for development/interactive usage

glossary: doc/Glossary.txt

image: interfaces/image.c

js: bin/RETRO12.html

repl: bin/retro-repl

ri: bin/retro-ri

update: bin/retro-unu literate/Unu.md literate/Muri.md
	./bin/retro-unu literate/Unu.md >tools/unu.c
	./bin/retro-unu literate/Muri.md >tools/muri.c

# File targets.

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

bin/retro-ri: interfaces/ri.c interfaces/image.c interfaces/io_filesystem.c
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro-ri $(LIBCURSES) io_filesystem.c ri.c

bin/retro: bin/retro-embedimage bin/retro-extend interfaces/image.c interfaces/rre.c interfaces/rre.forth interfaces/io_filesystem.c interfaces/io_filesystem.c interfaces/io_gopher.c interfaces/io_gopher.forth
	cp ngaImage cleanImage
	./bin/retro-extend interfaces/rre.forth
	./bin/retro-extend interfaces/io_filesystem.forth
	./bin/retro-extend interfaces/io_gopher.forth
	./bin/retro-embedimage >interfaces/rre_image_unix.c
	mv cleanImage ngaImage
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro $(LIBM) io_filesystem.c io_gopher.c rre.c

bin/retro-barebones: bin/retro-embedimage bin/retro-extend interfaces/image.c interfaces/barebones.c interfaces/barebones.forth
	cp ngaImage cleanImage
	./bin/retro-extend interfaces/barebones.forth
	./bin/retro-embedimage >interfaces/barebones_image.c
	mv cleanImage ngaImage
	cd interfaces && $(CC) $(CFLAGS) $(LDFLAGS) -o ../bin/retro-barebones barebones.c

bin/retro-unu: tools/unu.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/retro-unu tools/unu.c

doc/Glossary.txt: bin/retro words.tsv
	LC_ALL=C sort -o sorted.tsv words.tsv
	mv sorted.tsv words.tsv
	./bin/retro glossary.forth export glossary >doc/Glossary.txt

interfaces/image.c: bin/retro-embedimage bin/retro-extend bin/retro-muri literate/RetroForth.md literate/Rx.md
	./bin/retro-muri literate/Rx.md
	./bin/retro-extend literate/RetroForth.md
	./bin/retro-embedimage > interfaces/image.c
