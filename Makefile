CC := gcc
SRC := $(wildcard src/*.c)
CFLAGS := -ggdb -Wall -Wextra -Og
IFLAGS := -Iinclude
LFLAGS :=

.DEFAULT_GOAL := all

all: tools build asm

.PHONY: tools
tools:
	$(CC) $(CFLAGS) tools/opcode_table_generator.c -o bin/opcode-table-generator $(IFLAGS)
	$(CC) $(CFLAGS) src/cpu.c tools/6502_klaus_dormann_tests.c -o bin/6502-klaus-dormann-tests $(IFLAGS)

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
