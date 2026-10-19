#include "nes/cpu.h"

#include "nes/opcode_table.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct Machine Machine;
struct Machine {
    CPU cpu;
    u8 ram[0x10000];

    b8 should_log;
};

u8 bus_read(MemoryBus* bus, u16 address) {
    const Machine* machine = bus->ctx;
    u8 data = 0xFE;
    data = machine->ram[address];
    if (machine->should_log) {
        printf("READ(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, data, data);
    }
    return data;
}

void bus_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    if (address < 0x8000) {
        machine->ram[address] = value;
    } else {
        fprintf(stderr, "WARN: Can't write to ROM\n");
    }
    if (machine->should_log) {
        printf("WRITE(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, value, value);
    }
}

u16 step_instruction(Machine* machine) {
    CPU* cpu = &machine->cpu;
    assert(cpu->t == 0);

    u16 start_address = cpu->pc;
    if (machine->should_log) {
        printf("%s\n", OPCODE_STRING_TABLE[machine->ram[cpu->address_bus]]);
    }

    cpu_step(cpu);
    while (cpu->t != 0) {
        u8 curr_t = cpu->t;
        cpu_step(cpu);
        if (curr_t == cpu->t) {
            exit(1);
        }
    }

    if (machine->should_log) {
        print_cpu_status(cpu->p);
        printf("p = %d (0x%02X)\n", cpu->p, cpu->p);
        printf("a = %d (0x%02X)\n", cpu->a, cpu->a);
        printf("x = %d (0x%02X)\n", cpu->x, cpu->x);
        printf("s = %d (0x%02X)\n", cpu->s, cpu->s);
        printf("\n");
    }

    return start_address;
}

b8 run_test_file(const char* bin_filename, u16 done_address) {
    Machine machine = {
        // .should_log = true,
    };
    MemoryBus bus = {
        .ctx = &machine,
        .read = bus_read,
        .write = bus_write,
    };
    machine.cpu = cpu_init(bus);
    machine.cpu.interrupt_type = NONE;
    machine.cpu.pc = 0x0400;
    machine.cpu.address_bus = 0x0400;

    FILE* fp = fopen(bin_filename, "rb");
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(machine.ram, sizeof(u8), len, fp);
    fclose(fp);

    u16 last_instruction = 0xFFFF;
    while (true) {
        u16 current_instruction = step_instruction(&machine);
        if (last_instruction == current_instruction) {
            return false;
        }

        if (current_instruction == done_address) {
            return true;
        }

        last_instruction = current_instruction;
    }
}

i32 main(void) {
    b8 success = true;
    if (!run_test_file("asm/external/out/6502_functional_test.bin", 0x3469)) { success = false; }
    if (!run_test_file("asm/external/out/6502_decimal_test.bin", 0x044B)) { success = false; }
    return !success;
}
