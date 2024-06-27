CC = gcc
CFLAGS = -Wall -pthread

all: main

main: main.o
	$(CC) $(CFLAGS) -o main main.o -lm

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o main
