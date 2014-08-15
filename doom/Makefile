CC=gcc
FLAGS= -Wall -ansi -pedantic -g -pthread

all: server client

server: server.c
	$(CC) $^ $(FLAGS) -o $@

client: client.c
	$(CC) $^ $(FLAGS) -o $@ -lncurses

clean:
	rm -f server client
