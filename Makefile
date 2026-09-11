CC := gcc
SRC := $(wildcard src/*.c)
CFLAGS := -ggdb -Wall -Wextra -Og
IFLAGS := -Iinclude
LFLAGS :=

.DEFAULT := build

.PHONY: build
build: $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o bin/nes $(IFLAGS) $(LFLAGS)
