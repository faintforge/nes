CC := gcc
SRC := $(wildcard src/*.c)
CFLAGS := -ggdb -Wall -Wextra -Og
IFLAGS := -Iinclude
LFLAGS :=

.DEFAULT_GOAL := build

.PHONY: tools
tools:
	$(CC) $(CFLAGS) tools/opcode_table_generator.c -o bin/opcode-table-generator $(IFLAGS)

.PHONY: build
build: tools
	@mkdir -p bin
	./bin/opcode-table-generator > ./include/nes/opcode_table.h
	$(CC) $(CFLAGS) $(SRC) -o bin/nes $(IFLAGS) $(LFLAGS)
