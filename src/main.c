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

    FILE* fp = fopen("rom/test.bin", "rb");
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(cpu.memory+0x8000, len, 1, fp);
    fclose(fp);
    cpu.memory[0xFF00] = 255;

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
