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
    cpu->pc = 0x8000;
    cpu->address_bus = cpu->pc;
    cpu->bus_mode = READ;

    // machine.rom[0] = 0xA9; // LDA #imm
    // machine.rom[1] = 42;
    // machine.rom[0] = 0x91; // STA (ind),Y
    // machine.rom[1] = 0x1B;
    // machine.ram[0x1B] = 0x00;
    // machine.ram[0x1C] = 0x1B;
    // cpu->y = 2;
    // cpu->a = 42;

    machine.rom[0] = 0xAA;
    cpu->x = 0;
    cpu->a = 42;

    cpu_step(cpu);
    cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);
    // cpu_step(cpu);

    printf("a = %d ($%02X)\n", cpu->a, cpu->a);
    printf("x = %d ($%02X)\n", cpu->x, cpu->x);
    printf("%d\n", machine.ram[0x1B02]);

    return 0;
}
