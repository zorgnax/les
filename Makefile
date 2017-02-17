LDLIBS = -lncurses -liconv

all: les

%:
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o %.a %.so, $^) $(LDLIBS) -o $@

%.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(filter %.c, $^) -o $@

les: les.o charinfo.o readline2.o linewrap.o movement.o
les.o: les.c les.h
charinfo.o: charinfo.c les.h
readline2.o: readline2.c les.h
linewrap.o: linewrap.c les.h
movement.o: movement.c les.h

clean:
	rm -rf les *.o

.PHONY: all clean

