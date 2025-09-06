CC=gcc
CFLAGS=-std=c99 -O2 -Wall -Wextra -pedantic
INCLUDES=-Iinclude
SRC=src/main.c src/process.c src/queue.c src/scheduler.c
BIN=bin/os_sim

all: $(BIN)

$(BIN): $(SRC) include/process.h include/queue.h include/scheduler.h
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(BIN)

clean:
	rm -rf bin out

run:
	mkdir -p out
	./bin/os_sim -i data/processes.csv -a rr -q 3 -cs 1 -o out
