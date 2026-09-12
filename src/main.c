#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <stdio.h>

void pretty_print_opcode(u8 opcode) {
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s", opcode, op_pretty_string(op.type), addr_mode_pretty_string(op.addr_mode));
}

i32 main(void) {
    CPU cpu = cpu_create();

    u8 program[] = {
        0x18, // CLC
        0xA9, // LDA #imm
        0,
        0x85, // STA zpg
        1,
        0xDE, // DEC abs,X
        0x00,
        0xF1,
        0x4C, // JMP abs
        0xC,
        0x01,

        0xFF,

        0x20, // JSR abs
        0x10,
        0x01,

        0x00,
        0x00,
        0xFF,

        0xA9, // LDA #imm
        42,
        0x60, // RTS
    };

    size_t program_length = sizeof(program)/sizeof(program[0]);
    u16 starting_addr = 0x0100;
    for (size_t i = 0; i < program_length; i++) {
        cpu.memory[starting_addr + i] = program[i];
    }
    cpu.memory[0x00] = 0x40; // LDX #imm
    // cpu.memory[0x00] = 0xA2; // LDX #imm
    cpu.memory[0x01] = 0;
    cpu.memory[0x02] = 0x8E; // STX abs
    cpu.memory[0x03] = 0x00;
    cpu.memory[0x04] = 0xFF;
    cpu.memory[0xFF00] = 255;

    cpu.memory[0xFFFC] = starting_addr & 0xFF;
    cpu.memory[0xFFFD] = starting_addr >> 8;

    cpu_reset(&cpu);
    b8 running = true;
    i32 result = -1;
    while (running) {
        u16 addr = cpu.pc;
        u8 opcode = cpu.memory[addr];

        u8 cycles = cpu_step(&cpu);

        printf("%04X:    ", addr);
        pretty_print_opcode(opcode);
        printf("\tcycles: %u\n", cycles);
        if (cpu.memory[0xFF00] != 255) {
            running = false;
            result = cpu.memory[0xFF00];
        }
    }

    // printf("cycles = %lu\n", cpu.cycle);
    printf("a = %d\n", cpu.a);
    // printf("result = %d\n", result);
    // print_cpu_status(cpu.p);

    cpu_destroy(&cpu);
    return result;
}
