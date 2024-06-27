CC = gcc
CFLAGS = -Wall -pthread

all: MWCp

MWCp: 1901042656_main.o manager.o worker.o buffer.o transaction.o stats.o
	$(CC) $(CFLAGS) -o MWCp 1901042656_main.o manager.o worker.o buffer.o transaction.o stats.o

1901042656_main.o: 1901042656_main.c buffer.h transaction.h thread_args.h stats.h
	$(CC) $(CFLAGS) -c 1901042656_main.c

manager.o: manager.c buffer.h transaction.h thread_args.h
	$(CC) $(CFLAGS) -c manager.c

worker.o: worker.c buffer.h transaction.h thread_args.h stats.h
	$(CC) $(CFLAGS) -c worker.c

buffer.o: buffer.c buffer.h transaction.h
	$(CC) $(CFLAGS) -c buffer.c

transaction.o: transaction.c transaction.h
	$(CC) $(CFLAGS) -c transaction.c

stats.o: stats.c stats.h
	$(CC) $(CFLAGS) -c stats.c

clean:
	rm -f *.o MWCp
