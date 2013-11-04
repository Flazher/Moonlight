CFLAGS=-std=c99 -g -Wall -lX11 -lgiblib -fdump-rtl-expand
objects=main.o

all:	main.c
		make clean
		gcc -c main.c $(CFLAGS)
		gcc $(objects) -o wm $(CFLAGS)
clean:	
		rm -f *.o
