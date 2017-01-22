all: les

les: les.c
	gcc -o les les.c -lncurses -liconv

