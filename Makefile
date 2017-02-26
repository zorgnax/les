CC = gcc
LDLIBS = -lncurses -liconv
PREFIX = /usr/local

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

install:
	echo $(PREFIX)

clean:
	rm -rf les *.o

.PHONY: all clean install

