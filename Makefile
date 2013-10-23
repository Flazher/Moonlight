CFLAGS=-g -Wall -lX11
objects=main.o

all:	main.c init.c
		gcc -c main.c $(CFLAGS)
		gcc $(objects) -o wm $(CFLAGS)

clean:	
		rm -f *.o
