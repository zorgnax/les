CC = gcc
CFLAGS =
LDLIBS = -lncurses
include defines.mk

all: les

%:
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o %.a %.so, $^) $(LDLIBS) -o $@

%.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(filter %.c, $^) -o $@

les: main.o charinfo.o prompt.o linewrap.o movement.o stage.o page.o tabs.o readfile.o recentfiles.o search.o rx.o hash.o
main.o: main.c les.h
charinfo.o: charinfo.c les.h
prompt.o: prompt.c les.h
linewrap.o: linewrap.c les.h
movement.o: movement.c les.h
stage.o: stage.c les.h
page.o: page.c les.h
tabs.o: tabs.c les.h
readfile.o: readfile.c les.h
recentfiles.o: recentfiles.c les.h
search.o: search.c les.h
rx.o: rx.c rx.h
hash.o: hash.c hash.h

defines.mk:
	./configure

install: les
	mkdir -p $(PREFIX)/share/les
	install -c lespipe $(PREFIX)/share/les
	install -c les $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	install -c les.1 $(PREFIX)/share/man/man1

uninstall:
	rm -rf $(PREFIX)/share/les
	rm $(PREFIX)/bin/les

clean:
	rm -rf les *.o

clean2:
	rm -rf les *.o defines.h defines.mk


.PHONY: all install uninstall clean clean2

