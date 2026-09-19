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
    if (address < 0x8000) {
        return machine->ram[address];
    } else {
        return machine->rom[address - 0x8000];
    }
}

void bus_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    if (address < 0x8000) {
        machine->ram[address] = value;
    } else {
        fprintf(stderr, "ERR: Can't write to ROM\n");
    }
}

i32 main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [ROM_FILE]\n", argv[0]);
        return 1;
    }

    Machine machine = {0};
    MemoryBus bus = {
        .ctx = &machine,
        .read = bus_read,
        .write = bus_write,
    };
    machine.cpu = cpu_init(bus);

    return 0;
}
