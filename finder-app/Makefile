CC=aarch64-none-linux-gnu-gcc
GCC= gcc
FLAGS=-g -Wall -c

default: writer.c
	$(GCC) writer.c -o writer

all: writer.c
	$(GCC) writer.c -o writer
	$(CC) writer.c -o writer.o


CROSS_COMPILE:writer.c
#	$(CC) $(FLAGS) writer.c -o writer
	$(CC) writer.c -o writer.o

clean:
	rm -f writer *.o

