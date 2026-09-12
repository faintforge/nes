CC := gcc
SRC := $(wildcard src/*.c)
CFLAGS := -ggdb -Wall -Wextra -Og
IFLAGS := -Iinclude
LFLAGS :=

.DEFAULT_GOAL := all

all: tools build asm

.PHONY: tools
tools:
	$(CC) $(CFLAGS) src/op.c tools/opcode_table_generator.c -o bin/opcode-table-generator $(IFLAGS)

.PHONY: build
build: tools
	@mkdir -p bin
	./bin/opcode-table-generator > ./include/nes/opcode_table.h
	$(CC) $(CFLAGS) $(SRC) -o bin/nes $(IFLAGS) $(LFLAGS)

.PHONY: asm
asm:
	@mkdir -p tmp
	ca65 asm/test.s -o tmp/test.o
	ld65 tmp/test.o -C asm/config.ld -o rom/test.bin
	@rm -rf tmp/
