#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <stdio.h>

void pretty_print_opcode(u8 opcode) {
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s\n", opcode, op_pretty_string(op.type), addr_mode_pretty_string(op.addr_mode));
}

i32 main(void) {
    CPU cpu = cpu_create();

    u8 program[] = {
        0x18, // CLC
        0xA9, // LDA #imm
        0,
        0xE9, // SBC #imm
        1,
    };

    size_t program_length = sizeof(program)/sizeof(program[0]);
    cpu.pc = 0x0100;
    for (size_t i = 0; i < program_length; i++) {
        cpu.memory[cpu.pc + i] = program[i];
    }
    cpu.memory[cpu.pc + program_length + 0] = 0xA2; // LDX #imm
    cpu.memory[cpu.pc + program_length + 1] = 0;
    cpu.memory[cpu.pc + program_length + 2] = 0x8E; // STX abs
    cpu.memory[cpu.pc + program_length + 3] = 0x00;
    cpu.memory[cpu.pc + program_length + 4] = 0xFF;
    cpu.memory[0xFF00] = 255;

    b8 running = true;
    i32 result = -1;
    while (running) {
        printf("%04X:    ", cpu.pc);
        pretty_print_opcode(cpu.memory[cpu.pc]);

        // u64 cycles_start = cpu.cycle;
        cpu_step(&cpu);
        // printf("cycles: %lu\n", cpu.cycle - cycles_start);
        if (cpu.memory[0xFF00] != 255) {
            running = false;
            result = cpu.memory[0xFF00];
        }
    }

    printf("cycles = %lu\n", cpu.cycle);
    printf("a = %d\n", cpu.a);
    printf("result = %d\n", result);
    print_cpu_status(cpu.p);

    cpu_destroy(&cpu);
    return result;
}
