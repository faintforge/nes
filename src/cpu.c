#include "nes/cpu.h"
#include "nes/opcode_table.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MEMORY_SIZE 0x10000
// Stack = page 1 [0100-01FF]
#define STACK_START 0x0100

// =============================================================================
// Status flag helpers
// =============================================================================

static inline void cpu_set_status_flag(CPU* cpu, u8 flag, b8 value) {
    if (value) {
        cpu->p |= flag;
    } else {
        cpu->p &= ~flag;
    }
}

static inline b8 cpu_get_status_flag(CPU* cpu, u8 flag) {
    return (cpu->p & flag) != 0;
}


static inline void cpu_set_internal_status_flag(CPU* cpu, u8 flag, b8 value) {
    if (value) {
        cpu->internal_state |= flag;
    } else {
        cpu->internal_state &= ~flag;
    }
}

static inline b8 cpu_get_internal_status_flag(CPU* cpu, u8 flag) {
    return (cpu->internal_state & flag) != 0;
}

static inline void cpu_set_zero_negative(CPU* cpu, u8 value) {
    cpu_set_status_flag(cpu, FLAG_ZERO, value == 0);
    cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(value, 7));
}

// =============================================================================
// Memory operation helpers
// =============================================================================

// Go to next instruction
static void cpu_advance(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
    cpu->bus_mode = READ;
    cpu->t = 0;
    cpu->internal_state = 0;
}

// Advance PC only
static void cpu_advance_pc(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
}

// static u8 cpu_read(CPU* cpu, u16 address) {
//     cpu->bus.write(&cpu->bus, address, value);
// }
//
// static void cpu_write(CPU* cpu, u16 address, u8 value) {
//     cpu->bus.write(&cpu->bus, address, value);
// }
//
// static void stack_push(CPU* cpu, u8 value) {
//     cpu_write(cpu, STACK_START + cpu->s, value);
//     cpu->s--;
// }
//
// static u8 stack_pop(CPU* cpu) {
//     cpu->s++;
//     return cpu_read(cpu, STACK_START + cpu->s);
// }



void print_cpu_status(u8 status) {
    printf("CPU Status:\n");
    printf("    C = %d\n", (status >> 0) & 1);
    printf("    Z = %d\n", (status >> 1) & 1);
    printf("    I = %d\n", (status >> 2) & 1);
    printf("    D = %d\n", (status >> 3) & 1);
    printf("    V = %d\n", (status >> 6) & 1);
    printf("    N = %d\n", (status >> 7) & 1);
}

CPU cpu_init(MemoryBus bus) {
    // [Power up state](https://www.nesdev.org/wiki/CPU_power_up_state)
    // [Memory map](https://www.nesdev.org/wiki/CPU_memory_map)
    CPU cpu = {
        .bus = bus,
        .a = 0,
        .x = 0,
        .y = 0,
        .pc = 0xFFFC,
        .s = 0,
        .p = FLAG_INTERRUPT_DISABLE,
    };

    return cpu;
}

void step_addressing_mode(CPU* cpu, AddrMode addr_mode, u8 bus_op_type) {
    assert(cpu->t != 0);
    if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE)) {
        return;
    }

    switch (addr_mode) {
        case ADDR_MODE_ACCUMULATOR:
            UNREACHABLE();
        case ADDR_MODE_IMMEDIATE:
            UNREACHABLE();
        case ADDR_MODE_ZERO_PAGE:
            // Read effective address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_ZERO_PAGE_X:
            assert(cpu->t >= 1 && cpu->t <= 3);
            // Read effective address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu->t++;
            }
            // Read data (discarded)
            else if (cpu->t == 2) {
                u16 effective_address = cpu->address_bus + cpu->x;
                // Wrap at page boundary
                effective_address &= 0xFF;
                cpu->address_bus = effective_address;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_ZERO_PAGE_Y:
            assert(cpu->t >= 1 && cpu->t <= 3);
            // Read effective address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu->t++;
            }
            // Read data (discarded)
            else if (cpu->t == 2) {
                u16 effective_address = cpu->address_bus + cpu->y;
                // Wrap at page boundary
                effective_address &= 0xFF;
                cpu->address_bus = effective_address;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_ABSOLUTE:
            assert(cpu->t >= 1 && cpu->t <= 3);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance_pc(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u8 low = cpu->internal_data;
                u16 high = cpu->data_bus;
                cpu->address_bus = (high << 8) | low;
                cpu->t++;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
            }
            break;
        case ADDR_MODE_ABSOLUTE_X:
            assert(cpu->t >= 1 && cpu->t <= 4);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance_pc(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u16 low = cpu->internal_data + cpu->x;
                u16 high = cpu->data_bus;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, low <= 0xFF && bus_op_type == READ);
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_CARRY, low > 0xFF);
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_CARRY)) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_ABSOLUTE_Y:
            assert(cpu->t >= 1 && cpu->t <= 4);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance_pc(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u16 low = cpu->internal_data + cpu->y;
                u16 high = cpu->data_bus;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, low <= 0xFF && bus_op_type == READ);
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_CARRY, low > 0xFF);
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_CARRY)) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_INDIRECT_X:
            assert(cpu->t >= 1 && cpu->t <= 5);
            // t1: read zpg address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu->t++;
            }
            // t2: read data (discarded)
            else if (cpu->t == 2) {
                cpu->address_bus += cpu->x;
                // Page wrapping
                cpu->address_bus &= 0xFF;
                cpu->t++;
            }
            // t3: read adl
            else if (cpu->t == 3) {
                cpu->internal_data = cpu->data_bus;
                cpu->address_bus += 1;
                // Page wrapping
                cpu->address_bus &= 0xFF;
                cpu->t++;
            }
            // t4: read adh
            else if (cpu->t == 4) {
                u8 low = cpu->internal_data;
                u16 high = cpu->data_bus;
                cpu->address_bus = (high << 8) | low;
                cpu->t++;

                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
            }
            break;
        case ADDR_MODE_INDIRECT_Y:
            assert(cpu->t >= 1 && cpu->t <= 5);
            // t1: read zpg address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu->t++;
            }
            // t2: read adl
            else if (cpu->t == 2) {
                cpu->internal_data = cpu->data_bus;
                cpu->address_bus++;
                cpu->address_bus &= 0xFF;
                cpu->t++;
            }
            // t3: read adh
            else if (cpu->t == 3) {
                cpu->address_bus += 1;
                // Page wrapping
                cpu->address_bus &= 0xFF;
                u16 low = cpu->internal_data + cpu->y;
                u16 high = cpu->data_bus;
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, low <= 0xFF && bus_op_type == READ);
                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_CARRY, low > 0xFF);
            }
            // t4: read data
            else if (cpu->t == 4) {
                if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_CARRY)) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu_set_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE, true);
                cpu->t++;
            }
            break;
        case ADDR_MODE_IMPLIED:
            UNREACHABLE();
    }
}

void op_ld(CPU* cpu, Op op, u8* reg) {
    assert(cpu->t > 0);
    assert(cpu->bus_mode == READ);

    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        assert(cpu->t == 1);
        *reg = cpu->data_bus;
        cpu_set_zero_negative(cpu, *reg);

        cpu_advance(cpu);
        return;
    }

    if (!cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        *reg = cpu->data_bus;
        cpu_set_zero_negative(cpu, *reg);
        cpu_advance(cpu);
    }
}

void op_st(CPU* cpu, Op op, u8* reg) {
    assert(cpu->t > 0);

    // Addressing is done, instruction is done.
    if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE)) {
        cpu_advance(cpu);
        return;
    }

    step_addressing_mode(cpu, op.addr_mode, WRITE);

    if (cpu_get_internal_status_flag(cpu, FLAG_INTERNAL_ADDRESSING_DONE)) {
        cpu->data_bus = *reg;
        cpu->bus_mode = WRITE;
    }
}

void cpu_step(CPU* cpu) {
    // First phase of a cycle is always a memory operation.
    switch (cpu->bus_mode) {
        case READ:
            cpu->data_bus = cpu->bus.read(&cpu->bus, cpu->address_bus);
            break;
        case WRITE:
            cpu->bus.write(&cpu->bus, cpu->address_bus, cpu->data_bus);
            break;
        default:
            UNREACHABLE();
    }

    // t0: Fetch new opcode
    if (cpu->t == 0) {
        assert(cpu->bus_mode == READ);
        cpu->ir = cpu->data_bus;
        cpu_advance(cpu);
        cpu->t++;
        return;
    }

    // Step instruction
    Op op = OPCODE_TABLE[cpu->ir];
    switch (op.type) {
        // Access
        case OP_LDA:
            op_ld(cpu, op, &cpu->a);
            break;
        case OP_STA:
            op_st(cpu, op, &cpu->a);
            break;

        case OP_LDX:
            op_ld(cpu, op, &cpu->x);
            break;
        case OP_STX:
            op_st(cpu, op, &cpu->x);
            break;

        case OP_LDY:
            op_ld(cpu, op, &cpu->y);
            break;
        case OP_STY:
            op_st(cpu, op, &cpu->y);
            break;

        case OP__UNDEFINED:
            exit(1);

        default:
            UNREACHABLE();
    }

    cpu->cycle++;
}
