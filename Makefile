CC = gcc
LDLIBS = -lncurses -liconv
PREFIX = /usr/local
CPPFLAGS = -DPREFIX=\"$(PREFIX)\"

all: les

%:
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o %.a %.so, $^) $(LDLIBS) -o $@

%.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(filter %.c, $^) -o $@

les: les.o charinfo.o prompt.o linewrap.o movement.o stage.o
les.o: les.c les.h
charinfo.o: charinfo.c les.h
prompt.o: prompt.c les.h
linewrap.o: linewrap.c les.h
movement.o: movement.c les.h
stage.o: stage.c les.h

install: les
	mkdir -p $(PREFIX)/share/les
	install -c lesopen $(PREFIX)/share/les
	install -c les $(PREFIX)/bin

uninstall:
	rm -rf $(PREFIX)/share/les
	rm $(PREFIX)/bin/les

clean:
	rm -rf les *.o

.PHONY: all clean install uninstall

