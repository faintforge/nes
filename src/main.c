#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <stdio.h>

void pretty_print_opcode(u8 opcode) {
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s", opcode, op_pretty_string(op.type), addr_mode_pretty_string(op.addr_mode));
}

typedef struct SimpleBus SimpleBus;
struct SimpleBus {
    u8 memory[0x10000];
};

u8 simple_bus_read(MemoryBus* bus, u16 address) {
    SimpleBus* sbus = bus->ctx;
    return sbus->memory[address];
}

void simple_bus_write(MemoryBus* bus, u16 address, u8 value) {
    SimpleBus* sbus = bus->ctx;
    sbus->memory[address] = value;
}

i32 main(void) {
    SimpleBus sbus = {0};
    MemoryBus bus = {
        .ctx = &sbus,
        .read = simple_bus_read,
        .write = simple_bus_write,
    };
    CPU cpu = cpu_init(bus);

    FILE* fp = fopen("rom/test.bin", "rb");
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(sbus.memory+0x8000, len, 1, fp);
    fclose(fp);

    simple_bus_write(&bus, 0xFF00, 0xFF);

    cpu_reset(&cpu);
    b8 running = true;
    i32 result = -1;
    while (running) {
        u16 addr = cpu.pc;
        u8 opcode = cpu.bus.read(&cpu.bus, addr);

        u8 cycles = cpu_step(&cpu);

        printf("%04X:    ", addr);
        pretty_print_opcode(opcode);
        printf("\tcycles: %u\n", cycles);

        u8 value = cpu.bus.read(&cpu.bus, 0xFF00);
        if (value != 255) {
            running = false;
            result = value;
        }
    }

    return result;
}
