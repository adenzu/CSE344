CC = gcc
CFLAGS = -Wall -g
DEPS = shared.h eclist.h
OBJ = server.o client.o worker.o decliner.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: server client worker decliner

server: server.o
	$(CC) -o $@ $^ $(CFLAGS)

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

worker: worker.o
	$(CC) -o $@ $^ $(CFLAGS)

decliner: decliner.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o server client worker decliner
	rm -f /tmp/system_midterm_connection_request_mutex
	rm -f /tmp/system_midterm_archive_mutex_*
	rm -f /tmp/system_midterm_file_data_mutex_*
	rm -f /tmp/system_midterm_server_fifo_*
	rm -f /tmp/system_midterm_server_to_client_fifo_*
	rm -f /tmp/system_midterm_client_to_server_fifo_*
	rm -f /tmp/system_midterm_server_to_client_data_fifo_*
	rm -f /tmp/system_midterm_client_to_server_data_fifo_*
