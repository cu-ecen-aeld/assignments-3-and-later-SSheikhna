CC=aarch64-none-linux-gnu-gcc
GCC= gcc
FLAGS=-g -Wall -c

default: writer.c
	$(GCC) writer.c -o writer

CROSS_COMPILE:writer.c
	$(CC) $(FLAGS) writer.c -o writer.o

clean:
	rm -f writer *.o

