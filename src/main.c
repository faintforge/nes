#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <stdio.h>

void pretty_print_opcode(u8 opcode) {
    static const char* OP_PRETTY_STRING[OP__COUNT] = {
        "LDA", "STA", "LDX", "STX", "LDY", "STY", "TAX", "TXA", "TAY", "TYA",
        "ADC", "SBC", "INC", "DEC", "DEX", "INX", "INY", "DEY", "ASL", "LSR",
        "ROL", "ROR", "AND", "ORA", "EOR", "BIT", "CMP", "CPX", "CPY", "BCC",
        "BCS", "BEQ", "BNE", "BPL", "BMI", "BVC", "BVS", "JMP", "JSR", "RTS",
        "BRK", "RTI", "PHA", "PLA", "PHP", "PLP", "TXS", "TSX", "CLC", "SEC",
        "CLI", "SEI", "CLD", "SED", "CLV", "NOP",
    };

    static const char* ADDR_MODE_PRETTY_STRING[ADDR_MODE__COUNT] = {
        "A",     "#imm", "zpg",   "zpg,X", "zpg,Y", "abs", "abs,X",
        "abs,Y", "(ind)",  "(ind,X)", "(ind),Y", "rel",   "",
    };
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s\n", opcode, OP_PRETTY_STRING[op.type], ADDR_MODE_PRETTY_STRING[op.addr_mode]);
}

i32 main(void) {
    CPU cpu = cpu_create();

    cpu.pc = 0x0100;
    cpu.memory[0x0100] = 0xA9; // LDA #imm
    cpu.memory[0x0101] = 42;
    cpu.memory[0x0102] = 0xA2; // LDX #imm
    cpu.memory[0x0103] = 0;
    cpu.memory[0x0104] = 0x8E; // STX abs
    cpu.memory[0x0105] = 0x00;
    cpu.memory[0x0106] = 0xFF;
    cpu.memory[0xFF00] = 255;

    b8 running = true;
    i32 result = -1;
    while (running) {
        printf("%04X:    ", cpu.pc);
        pretty_print_opcode(cpu.memory[cpu.pc]);
        cpu_step(&cpu);
        if (cpu.memory[0xFF00] != 255) {
            running = false;
            result = cpu.memory[0xFF00];
        }
    }

    printf("cycles = %lu\n", cpu.cycle);
    printf("result = %d\n", result);

    cpu_destroy(&cpu);
    return result;
}
