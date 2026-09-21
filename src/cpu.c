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

// Go to next instruction
static void cpu_advance(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
    cpu->bus_mode = READ;
    cpu->t = 0;
    cpu->internal_carry = 0;
    cpu->addr_ready_t = 0;
}

// Advance PC only
static void cpu_advance_pc(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
}

static b8 cpu_is_addr_ready(CPU* cpu) {
    return cpu->addr_ready_t != 0;
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
    const char states[16] = {
        'c', 'C', // Carry
        'z', 'Z', // Zero
        'i', 'I', // Interrupt
        'd', 'D', // Decimal
        'b', 'B', // Break
        '_', '_', // Decimal
        'v', 'V', // Overflow
        'n', 'N', // Negative
    };

    printf("CPU Status: ");
    for (u8 i = 0; i < 8; i++) {
        i32 idx = i*2 + ((status >> i) & 1);
        putchar(states[idx]);
    }
    putchar('\n');
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
    if (cpu_is_addr_ready(cpu)) {
        return;
    }

    switch (addr_mode) {
        case ADDR_MODE_ACCUMULATOR:
            UNREACHABLE();
        case ADDR_MODE_IMMEDIATE:
            cpu->addr_ready_t = cpu->t;
            cpu->t++;
            break;
            // UNREACHABLE();
        case ADDR_MODE_ZERO_PAGE:
            // Read effective address
            if (cpu->t == 1) {
                cpu->address_bus = cpu->data_bus;

                cpu->addr_ready_t = cpu->t;
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

                cpu->addr_ready_t = cpu->t;
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

                cpu->addr_ready_t = cpu->t;
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

                cpu->addr_ready_t = cpu->t;
                cpu->t++;
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
                cpu->internal_carry = low > 0xFF;
                cpu->address_bus = (high << 8) | (low & 0xFF);

                if (low <= 0xFF) {
                    cpu->addr_ready_t = cpu->t;
                }
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu->internal_carry) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu->addr_ready_t = cpu->t;
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
                cpu->internal_carry = low > 0xFF;
                cpu->address_bus = (high << 8) | (low & 0xFF);

                if (low <= 0xFF && bus_op_type == READ) {
                    cpu->addr_ready_t = cpu->t;
                }
                cpu->t++;
            } else if (cpu->t == 3) {
                if (cpu->internal_carry) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu->addr_ready_t = cpu->t;
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

                cpu->addr_ready_t = cpu->t;
                cpu->t++;
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
                cpu->internal_carry = low > 0xFF;

                if (low <= 0xFF && bus_op_type == READ) {
                    cpu->addr_ready_t = cpu->t;
                }
                cpu->t++;
            }
            // t4: read data
            else if (cpu->t == 4) {
                if (cpu->internal_carry) {
                    // Add one page to address
                    cpu->address_bus += 0x0100;
                }

                cpu->addr_ready_t = cpu->t;
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

    step_addressing_mode(cpu, op.addr_mode, READ);
    if (cpu->addr_ready_t != 0) {
        *reg = cpu->data_bus;
        cpu_set_zero_negative(cpu, *reg);
        cpu_advance(cpu);
    }
}

void op_st(CPU* cpu, Op op, u8* reg) {
    assert(cpu->t > 0);

    // Addressing is done, instruction is done.
    if (cpu_is_addr_ready(cpu)) {
        cpu_advance(cpu);
        return;
    }

    step_addressing_mode(cpu, op.addr_mode, WRITE);

    if (cpu_is_addr_ready(cpu)) {
        cpu->data_bus = *reg;
        cpu->bus_mode = WRITE;
    }
}

void op_transfer(CPU* cpu, Op op, u8 left, u8* right) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *right = left;
    cpu_set_zero_negative(cpu, *right);

    cpu->pc--;
    cpu_advance(cpu);
}

void op_adc(CPU* cpu, Op op) {
    assert(cpu->t > 0);

    step_addressing_mode(cpu, op.addr_mode, READ);

    if (cpu_is_addr_ready(cpu)) {
        u16 result = cpu->a + cpu->data_bus + cpu_get_status_flag(cpu, FLAG_CARRY);

        u8 result_sign = get_bit(result, 7);
        u8 a_sign = get_bit(cpu->a, 7);
        u8 memory_sign = get_bit(cpu->data_bus, 7);
        b8 overflow = result_sign != a_sign && result_sign != memory_sign;

        cpu->a = result;

        cpu_set_status_flag(cpu, FLAG_CARRY, result > 0xFF);
        cpu_set_status_flag(cpu, FLAG_OVERFLOW, overflow);
        cpu_set_zero_negative(cpu, result);

        cpu_advance(cpu);
    }
}

void op_sbc(CPU* cpu, Op op) {
    assert(cpu->t > 0);

    step_addressing_mode(cpu, op.addr_mode, READ);

    if (cpu_is_addr_ready(cpu)) {
        i8 result = cpu->a + ~cpu->data_bus + cpu_get_status_flag(cpu, FLAG_CARRY);

        u8 result_sign = get_bit(result, 7);
        u8 a_sign = get_bit(cpu->a, 7);
        u8 memory_sign = get_bit(cpu->data_bus, 7);
        b8 overflow = result_sign != a_sign && result_sign != memory_sign;

        cpu->a = result;

        cpu_set_status_flag(cpu, FLAG_CARRY, result >= 0);
        cpu_set_status_flag(cpu, FLAG_OVERFLOW, overflow);
        cpu_set_zero_negative(cpu, result);

        cpu_advance(cpu);
    }
}

void op_inc(CPU* cpu, Op op) {
    if (cpu_is_addr_ready(cpu)) {
        if (cpu->t == cpu->addr_ready_t + 1) {
            // cpu->data_bus = cpu->data_bus;
            cpu->bus_mode = WRITE;
            cpu->t++;
        } else if (cpu->t == cpu->addr_ready_t + 2) {
            cpu->data_bus++;
            cpu->bus_mode = WRITE;
            cpu_set_zero_negative(cpu, cpu->data_bus);
            cpu->t++;
        } else if (cpu->t == cpu->addr_ready_t + 3) {
            cpu_advance(cpu);
        }
    } else {
        step_addressing_mode(cpu, op.addr_mode, WRITE);
    }
}

void op_dec(CPU* cpu, Op op) {
    if (cpu_is_addr_ready(cpu)) {
        if (cpu->t == cpu->addr_ready_t + 1) {
            // cpu->data_bus = cpu->data_bus;
            cpu->bus_mode = WRITE;
            cpu->t++;
        } else if (cpu->t == cpu->addr_ready_t + 2) {
            cpu->data_bus--;
            cpu->bus_mode = WRITE;
            cpu_set_zero_negative(cpu, cpu->data_bus);
            cpu->t++;
        } else if (cpu->t == cpu->addr_ready_t + 3) {
            cpu_advance(cpu);
        }
    } else {
        step_addressing_mode(cpu, op.addr_mode, WRITE);
    }
}

void op_inc_register(CPU* cpu, Op op, u8* reg) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *reg += 1;
    cpu_set_zero_negative(cpu, *reg);

    // Implied addressing doesn't read an operand so decrement pc once as to
    // not skip the next instruction.
    cpu->pc--;
    cpu_advance(cpu);
}

void op_dec_register(CPU* cpu, Op op, u8* reg) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *reg -= 1;
    cpu_set_zero_negative(cpu, *reg);

    cpu->pc--;
    cpu_advance(cpu);
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

        // Transfer
        case OP_TAX:
            op_transfer(cpu, op, cpu->a, &cpu->x);
            break;
        case OP_TXA:
            op_transfer(cpu, op, cpu->x, &cpu->a);
            break;
        case OP_TAY:
            op_transfer(cpu, op, cpu->a, &cpu->y);
            break;
        case OP_TYA:
            op_transfer(cpu, op, cpu->y, &cpu->a);
            break;

        // Arithmetic
        case OP_ADC:
            op_adc(cpu, op);
            break;
        case OP_SBC:
            op_sbc(cpu, op);
            break;
        case OP_INC:
            op_inc(cpu, op);
            break;
        case OP_DEC:
            op_dec(cpu, op);
            break;
        case OP_INX:
            op_inc_register(cpu, op, &cpu->x);
            break;
        case OP_DEX:
            op_dec_register(cpu, op, &cpu->x);
            break;
        case OP_INY:
            op_inc_register(cpu, op, &cpu->y);
            break;
        case OP_DEY:
            op_dec_register(cpu, op, &cpu->y);
            break;

        case OP__UNDEFINED:
            exit(1);

        default:
            UNREACHABLE();
    }

    cpu->cycle++;
}
