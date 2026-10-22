CC := gcc
SRC := $(wildcard src/*.c)
CFLAGS := -ggdb -Wall -Wextra -Og
IFLAGS := -Iinclude -Ilibs/glad/include
LFLAGS := -lm -lSDL3

# CFLAGS += -fsanitize=address -fno-omit-frame-pointer

.DEFAULT_GOAL := all

all: tools build asm

.PHONY: tools
tools:
	$(CC) $(CFLAGS) tools/opcode_table_generator.c -o bin/opcode-table-generator $(IFLAGS)
	$(CC) $(CFLAGS) src/cpu.c tools/6502_klaus_dormann_tests.c -o bin/6502-klaus-dormann-tests $(IFLAGS)

.PHONY: build
build: tools libs/libs.o
	@mkdir -p bin
	./bin/opcode-table-generator > ./include/nes/opcode_table.h
	$(CC) $(CFLAGS) $(SRC) libs/libs.o -o bin/nes $(IFLAGS) $(LFLAGS)

.PHONY: asm
asm:
	@mkdir -p tmp
	ca65 asm/test.s -o tmp/test.o
	ld65 tmp/test.o -C asm/config.ld -o rom/test.bin
	@rm -rf tmp/

libs/libs.o: libs/glad/src/gl.c
	$(CC) -O3 -c $< -o $@ $(IFLAGS)
