#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void pretty_print_opcode(u8 opcode) {
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s", opcode, op_pretty_string(op.type), addr_mode_pretty_string(op.addr_mode));
}

typedef struct TestingMachine TestingMachine;
struct TestingMachine {
    CPU cpu;
    u8 memory[0x10000];
    b8 running;
    u8 result;
};

u8 bus_read(MemoryBus* bus, u16 address) {
    TestingMachine* machine = bus->ctx;
    return machine->memory[address];
}

void dump_cpu(const CPU* cpu) {
    const char status_flags[16] = {
        'c', 'C',
        'z', 'Z',
        'i', 'I',
        'd', 'D',
        'b', 'B',
        ' ', ' ',
        'o', 'O',
        'n', 'N',
    };

    printf("\n===== CPU DUMP =====\n");
    printf("A  = %-5u (0x%02X)\n", cpu->a, cpu->a);
    printf("X  = %-5u (0x%02X)\n", cpu->x, cpu->x);
    printf("Y  = %-5u (0x%02X)\n", cpu->y, cpu->y);
    printf("PC = %-5u (0x%04X)\n", cpu->pc, cpu->pc);
    printf("S  = %-5u (0x%02X)\n", cpu->s, cpu->s);
    printf("P  = ");
    for (u8 i = 0; i < 8; i++) {
        u8 value = (cpu->p >> i) & 1;
        putc(status_flags[i*2 + value], stdout);
    }
    printf("\n");
    printf("====================\n\n");
}

void bus_write(MemoryBus* bus, u16 address, u8 value) {
    TestingMachine* machine = bus->ctx;
    if (address >= 0x4000 && address <= 0x7FFF) {
        switch (address) {
            // Reset machine state but keep executing
            case 0x4000:
                memset(machine->memory, 0, 0x8000);
                CPU* cpu = &machine->cpu;
                cpu->p = CPU_STATUS_INTERRUPT_DISABLE;
                cpu->a = 0;
                cpu->x = 0;
                cpu->y = 0;
                cpu->s = 0xFD;
                break;
            case 0x4001:
                dump_cpu(&machine->cpu);
                break;
            case 0x7FFF:
                machine->result = value;
                machine->running = false;
                break;
        }
    }

    if (address >= 0x8000) {
        fprintf(stderr, "ERR: Attempted to write to ROM [0x8000-0xFFFF]");
        exit(1);
    }

    machine->memory[address] = value;
}

b8 load_rom(TestingMachine* machine, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERR: Failed to read %s: %s\n", filename, strerror(errno));
        return false;
    }
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(machine->memory+0x8000, len, 1, fp);
    fclose(fp);
    return true;
}

i32 main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [ROM_FILE]\n", argv[0]);
        return 1;
    }

    TestingMachine machine = {
        .running = true,
    };
    MemoryBus bus = {
        .ctx = &machine,
        .read = bus_read,
        .write = bus_write,
    };
    machine.cpu = cpu_init(bus);
    if (!load_rom(&machine, argv[1])) {
        return 1;
    }

    cpu_reset(&machine.cpu);
    while (machine.running) {
        u16 addr = machine.cpu.pc;
        u8 opcode = bus_read(&bus, addr);

        u8 cycles = cpu_step(&machine.cpu);

        printf("%04X:    ", addr);
        pretty_print_opcode(opcode);
        printf("\tcycles: %u\n", cycles);

        if ((machine.cpu.p & CPU_STATUS_BREAK) != 0) {
            fprintf(stderr, "ERR: Break flag set in CPU.");
        }
    }

    return machine.result;
}
