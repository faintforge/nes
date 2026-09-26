#include "nes/cpu.h"

#include <stdio.h>

typedef struct Machine Machine;
struct Machine {
    CPU cpu;
    u8 ram[0x8000];
    u8 rom[0x8000];
};

u8 bus_read(MemoryBus* bus, u16 address) {
    const Machine* machine = bus->ctx;
    u8 data = 0xFE;
    if (address < 0x8000) {
        data = machine->ram[address];
    } else {
        data = machine->rom[address - 0x8000];
    }
    printf("READ(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, data, data);
    return data;
}

void bus_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    if (address < 0x8000) {
        machine->ram[address] = value;
    } else {
        // fprintf(stderr, "ERR: Can't write to ROM\n");
    }
    printf("WRITE(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, value, value);
}

i32 main(void) {
    // if (argc != 2) {
    //     fprintf(stderr, "Usage: %s [ROM_FILE]\n", argv[0]);
    //     return 1;
    // }

    Machine machine = {0};
    MemoryBus bus = {
        .ctx = &machine,
        .read = bus_read,
        .write = bus_write,
    };
    machine.cpu = cpu_init(bus);
    CPU* cpu = &machine.cpu;
    cpu->pc = 0x8080;
    cpu->address_bus = cpu->pc;
    cpu->s = 0xFF;
    cpu->interrupt_type = NONE;

    #define OFFSET ((i8) 127)
    // printf("%02X\n", 0x82 + OFFSET);
    machine.rom[0x80] = 0x90; // BCC rel
    machine.rom[0x81] = OFFSET;
    machine.rom[0x82 + OFFSET] = 0xEA;

    cpu_step(cpu);
    cpu_step(cpu);
    cpu_step(cpu);
    cpu_step(cpu);
    printf("\n");

    cpu_step(cpu);

    printf("x = %d ($%02X)\n", cpu->x, cpu->x);
    printf("%d\n", machine.ram[0x1B]);
    print_cpu_status(cpu->p);
    // printf("%d\n", cpu->x);

    return 0;
}
