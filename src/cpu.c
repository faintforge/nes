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

static inline void cpu_set_zero_negative(CPU* cpu, u8 value) {
    cpu_set_status_flag(cpu, FLAG_ZERO, value == 0);
    cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(value, 7));
}

// =============================================================================
// Memory operation helpers
// =============================================================================

static void cpu_advance(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
    cpu->bus_mode = READ;
}

static u8 cpu_read(CPU* cpu, u16 address) {
}

static void cpu_write(CPU* cpu, u16 address, u8 value) {
    cpu->bus.write(&cpu->bus, address, value);
}

static void stack_push(CPU* cpu, u8 value) {
    cpu_write(cpu, STACK_START + cpu->s, value);
    cpu->s--;
}

static u8 stack_pop(CPU* cpu) {
    cpu->s++;
    return cpu_read(cpu, STACK_START + cpu->s);
}



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

// void step_address_mode(CPU* cpu, Op op, u8 data) {
//     switch (op.addr_mode) {
//         case ADDR_MODE_ACCUMULATOR:
//             break;
//         case ADDR_MODE_IMMEDIATE:
//             assert(cpu->bus_mode == READ);
//             break;
//         case ADDR_MODE_ZERO_PAGE:
//             break;
//         case ADDR_MODE_ZERO_PAGE_X:
//             break;
//         case ADDR_MODE_ZERO_PAGE_Y:
//             break;
//         case ADDR_MODE_ABSOLUTE:
//             break;
//         case ADDR_MODE_ABSOLUTE_X:
//             break;
//         case ADDR_MODE_ABSOLUTE_Y:
//             break;
//         case ADDR_MODE_INDIRECT:
//             break;
//         case ADDR_MODE_INDIRECT_X:
//             break;
//         case ADDR_MODE_INDIRECT_Y:
//             break;
//         case ADDR_MODE_RELATIVE:
//             break;
//         case ADDR_MODE_IMPLIED:
//             break;
//         default:
//             UNREACHABLE();
//     }
// }

void op_ld(CPU* cpu, Op op, u8* reg) {
    assert(op.type == OP_LDA);
    assert(cpu->t > 0);
    assert(cpu->bus_mode == READ);

    switch (op.addr_mode) {
        case ADDR_MODE_IMMEDIATE:
            assert(cpu->t == 1);
            *reg = cpu->data_bus;
            cpu_set_zero_negative(cpu, *reg);

            cpu_advance(cpu);
            cpu->t = 0;
            break;
        case ADDR_MODE_ZERO_PAGE:
            assert(cpu->t >= 1 && cpu->t <= 2);
            // t1: Read effective address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;
                cpu->t++;
            }
            // t2: Read data
            else if (cpu->t == 2) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);

                cpu_advance(cpu);
                cpu->t = 0;
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

                cpu->t++;
            }
            // Read data
            else if (cpu->t == 3) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);

                cpu_advance(cpu);
                cpu->t = 0;
            }
            break;
        case ADDR_MODE_ABSOLUTE:
            assert(cpu->t >= 1 && cpu->t <= 3);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u8 low = cpu->internal_data;
                u16 high = cpu->data_bus;
                cpu->address_bus = (high << 8) | low;
                cpu->t++;
            } else if (cpu->t == 3) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);

                cpu_advance(cpu);
                cpu->t = 0;
            }
            break;
        case ADDR_MODE_ABSOLUTE_X:
            assert(cpu->t >= 1 && cpu->t <= 4);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u16 low = cpu->internal_data + cpu->x;
                u16 high = cpu->data_bus;
                cpu->internal_carry = low > 0xFF;
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu->internal_carry == 0) {
                    *reg = cpu->data_bus;
                    cpu_set_zero_negative(cpu, *reg);
                    cpu_advance(cpu);
                    cpu->t = 0;
                } else {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                    cpu->t++;
                }
            } else if (cpu->t == 4) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);
                cpu_advance(cpu);
                cpu->t = 0;
            }
            break;
        case ADDR_MODE_ABSOLUTE_Y:
            assert(cpu->t >= 1 && cpu->t <= 4);
            // t1: read addres low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance(cpu);
                cpu->t++;
            }
            // t2: read address high
            else if (cpu->t == 2) {
                u16 low = cpu->internal_data + cpu->y;
                cpu->internal_carry = low > 0xFF;
                u16 high = cpu->data_bus;
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu->internal_carry == 0) {
                    *reg = cpu->data_bus;
                    cpu_set_zero_negative(cpu, *reg);
                    cpu_advance(cpu);
                    cpu->t = 0;
                } else {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                    cpu->t++;
                }
            } else if (cpu->t == 4) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);
                cpu_advance(cpu);
                cpu->t = 0;
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
            }
            // t5: read data
            else if (cpu->t == 5) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);
                cpu_advance(cpu);
                cpu->t = 0;
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
                cpu->internal_carry = low > 0xFF;
                cpu->address_bus = (high << 8) | (low & 0xFF);
                cpu->t++;
            }
            // t4: read data
            else if (cpu->t == 4) {
                if (cpu->internal_carry == 0) {
                    *reg = cpu->data_bus;
                    cpu_set_zero_negative(cpu, *reg);
                    cpu_advance(cpu);
                    cpu->t = 0;
                } else {
                    cpu->address_bus += 0x0100;
                    cpu->t++;
                }
            }
            // t5: read data (page-boundary crossed)
            else if (cpu->t == 5) {
                *reg = cpu->data_bus;
                cpu_set_zero_negative(cpu, *reg);
                cpu_advance(cpu);
                cpu->t = 0;
            }
            break;
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
        case OP_LDA:
            op_ld(cpu, op, &cpu->a);
            break;
        case OP__UNDEFINED:
            exit(1);
    }

    cpu->cycle++;
}
