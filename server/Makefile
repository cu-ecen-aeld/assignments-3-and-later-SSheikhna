CC=aarch64-none-linux-gnu-gcc
GCC= gcc
FLAGS=-g -Wall -c

default: aesdsocket.c
	$(GCC) aesdsocket.c -o aesdsocket

all: aesdsocket.c
	$(GCC) aesdsocket.c -o aesdsocket
	$(CC) aesdsocket.c -o aesdsocket.o


CROSS_COMPILE:aesdsocket.c
#	$(CC) $(FLAGS) writer.c -o writer
	$(CC) aesdsocket.c -o aesdsocket.o

clean:
	rm -f aesdsocket *.o

