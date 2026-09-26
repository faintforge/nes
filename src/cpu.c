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

    if (cpu->reset != 0) {
        cpu->interrupt_type = RESET;
    } else if (cpu->nmi != 0) {
        cpu->interrupt_type = NMI;
    } else if (cpu->irq != 0 && !cpu_get_status_flag(cpu, FLAG_INTERRUPT_DISABLE)) {
        cpu->interrupt_type = IRQ;
    }
}

// Advance PC only
static void cpu_advance_pc(CPU* cpu) {
    cpu->pc++;
    cpu->address_bus = cpu->pc;
}

static b8 cpu_is_addr_ready(CPU* cpu) {
    return cpu->addr_ready_t != 0;
}



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
        .pc = 0x0000,
        .s = 0,
        .p = FLAG_INTERRUPT_DISABLE,
        .bus_mode = READ,
        .interrupt_type = RESET,
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
        case ADDR_MODE_INDIRECT:
            assert(cpu->t >= 1 && cpu->t <= 5);
            // t1: read op low
            if (cpu->t == 1) {
                cpu->internal_data = cpu->data_bus;
                cpu_advance_pc(cpu);
                cpu->t++;
            }
            // t2: read op high
            else if (cpu->t == 2) {
                cpu->address_bus = ((u16) cpu->data_bus << 8) | cpu->internal_data;
                cpu->t++;
            }
            // t3: read adl
            else if (cpu->t == 3) {
                cpu->internal_data = cpu->data_bus;
                // NOTE: This page wrapping behavior only occurs on the
                // NMOS 6502. The CMOS 65C02 fixes this.
                u16 high = cpu->address_bus >> 8;
                cpu->address_bus += 1;
                // Page wrapping
                cpu->address_bus &= 0xFF;
                cpu->address_bus |= high << 8;
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
        default:
            UNREACHABLE();
    }
}

void op_ld(CPU* cpu, Op op, u8* reg) {
    assert(cpu->t > 0);
    assert(cpu->bus_mode == READ);

    // TODO: Figure out if this is actually correct or if it fails at immediate
    // addressing.
    step_addressing_mode(cpu, op.addr_mode, READ);
    if (cpu_is_addr_ready(cpu)) {
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

typedef void (*rwm_func)(CPU* cpu, u8* memory);

void op_read_modify_write(CPU* cpu, Op op, rwm_func func) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        func(cpu, &cpu->a);
        cpu_advance(cpu);
    } else {
        if (!cpu_is_addr_ready(cpu)) {
            step_addressing_mode(cpu, op.addr_mode, WRITE);
        } else {
            if (cpu->t == cpu->addr_ready_t + 1) {
                // cpu->data_bus = cpu->data_bus;
                cpu->bus_mode = WRITE;
                cpu->t++;
            } else if (cpu->t == cpu->addr_ready_t + 2) {
                cpu->bus_mode = WRITE;
                func(cpu, &cpu->data_bus);
                cpu->t++;
            } else if (cpu->t == cpu->addr_ready_t + 3) {
                cpu_advance(cpu);
            }
        }
    }
}

void rmw_inc(CPU* cpu, u8* memory) {
    (*memory)++;
    cpu_set_zero_negative(cpu, *memory);
}

void rmw_dec(CPU* cpu, u8* memory) {
    (*memory)--;
    cpu_set_zero_negative(cpu, cpu->data_bus);
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

void rmw_asl(CPU* cpu, u8* memory) {
    b8 carry = get_bit(*memory, 7);
    *memory <<= 1;
    cpu_set_zero_negative(cpu, *memory);
    cpu_set_status_flag(cpu, FLAG_CARRY, carry);
}

void rmw_lsr(CPU* cpu, u8* memory) {
    b8 carry = *memory & 1;
    *memory >>= 1;
    cpu_set_zero_negative(cpu, *memory);
    cpu_set_status_flag(cpu, FLAG_CARRY, carry);
}

void rmw_rol(CPU* cpu, u8* memory) {
    u8 carry = cpu_get_status_flag(cpu, FLAG_CARRY);
    b8 new_carry = get_bit(*memory, 7);
    *memory = (*memory << 1) | carry;
    cpu_set_zero_negative(cpu, *memory);
    cpu_set_status_flag(cpu, FLAG_CARRY, new_carry);
}

void rmw_ror(CPU* cpu, u8* memory) {
    u8 carry = cpu_get_status_flag(cpu, FLAG_CARRY);
    b8 new_carry = *memory & 1;
    *memory = (*memory >> 1) | (carry << 7);
    cpu_set_zero_negative(cpu, *memory);
    cpu_set_status_flag(cpu, FLAG_CARRY, new_carry);
}

void op_and(CPU* cpu, Op op) {
    // TODO: Figure out a nicer way to handle immediate addressing.
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        cpu->a &= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        cpu->a &= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
    }
}

void op_ora(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        cpu->a |= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        cpu->a |= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
    }
}

void op_eor(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        cpu->a ^= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        cpu->a ^= cpu->data_bus;
        cpu_set_zero_negative(cpu, cpu->a);
        cpu_advance(cpu);
    }
}

void op_bit(CPU* cpu, Op op) {
    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        u8 result = cpu->a & cpu->data_bus;
        cpu_set_zero_negative(cpu, result);
        cpu_set_status_flag(cpu, FLAG_OVERFLOW, get_bit(result, 6));
        cpu_advance(cpu);
    }
}

void op_cmp(CPU* cpu, Op op, u8 reg) {
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        cpu_set_status_flag(cpu, FLAG_CARRY, reg >= cpu->data_bus);
        cpu_set_status_flag(cpu, FLAG_ZERO, reg == cpu->data_bus);
        cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(reg - cpu->data_bus, 7));
        cpu_advance(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, READ);
    } else {
        cpu_set_status_flag(cpu, FLAG_CARRY, reg >= cpu->data_bus);
        cpu_set_status_flag(cpu, FLAG_ZERO, reg == cpu->data_bus);
        cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(reg - cpu->data_bus, 7));
        cpu_advance(cpu);
    }
}

void op_jmp(CPU* cpu, Op op) {
    step_addressing_mode(cpu, op.addr_mode, READ);
    if (cpu_is_addr_ready(cpu)) {
        u16 address = cpu->address_bus;
        cpu_advance(cpu);
        cpu->pc = address;
        cpu->address_bus = address;
    }
}

void op_jsr(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_ABSOLUTE);

    switch (cpu->t) {
        // Fetch adl
        case 1:
            cpu->internal_data = cpu->data_bus;
            cpu_advance_pc(cpu);
            // Jump to stack
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        // Push pch
        case 2:
            cpu->bus_mode = WRITE;
            cpu->data_bus = cpu->pc >> 8;
            cpu->t++;
            break;
        // Push pcl
        case 3:
            cpu->bus_mode = WRITE;
            cpu->data_bus = cpu->pc & 0xFF;
            cpu->s--;
            // Stack page wrap around behavior
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 4:
            cpu->s--;
            cpu->bus_mode = READ;
            cpu->address_bus = cpu->pc;
            cpu->t++;
            break;
        // Fetch adh
        case 5:
            cpu_advance(cpu);
            cpu->address_bus = ((u16) cpu->data_bus << 8) | cpu->internal_data;
            cpu->pc = cpu->address_bus;
            break;
        default:
            UNREACHABLE();
    }
}

void op_rts(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);

    switch (cpu->t) {
        // Fetch adl
        case 1:
            // Jump to stack
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 2:
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 3:
            // Fetch pcl
            cpu->internal_data = cpu->data_bus;
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 4:
            // Fetch pch
            cpu->address_bus = ((u16) cpu->data_bus << 8) | cpu->internal_data;
            cpu->pc = cpu->address_bus;
            cpu->t++;
            break;
        case 5:
            cpu_advance(cpu);
            break;
        default:
            UNREACHABLE();
    }
}

void op_brk(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);

    switch (cpu->t) {
        // Push pch
        case 1:
            cpu_advance_pc(cpu);
            cpu->bus_mode = WRITE;
            cpu->data_bus = cpu->pc >> 8;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->s--;
            cpu->t++;
            break;
        // Push pcl
        case 2:
            // TODO: Check if we should advance the PC when servicing a non-brk
            // interrupt request.
            cpu_advance_pc(cpu);
            cpu->bus_mode = WRITE;
            cpu->data_bus = cpu->pc & 0xFF;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->s--;
            cpu->t++;
            break;
        // Push p
        case 3:
            cpu->bus_mode = WRITE;
            if (cpu->interrupt_type == BRK) {
                cpu->data_bus = cpu->p | FLAG_BREAK;
            } else {
                cpu->data_bus = cpu->p;
            }
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->s--;
            cpu->t++;
            break;
        // Fetch adl
        case 4:
            cpu_set_status_flag(cpu, FLAG_INTERRUPT_DISABLE, true);
            cpu->bus_mode = READ;
            switch (cpu->interrupt_type) {
                case NMI:
                    cpu->address_bus = 0xFFFA;
                    break;
                case RESET:
                    cpu->address_bus = 0xFFFC;
                    break;
                // fallthrough
                case IRQ:
                case BRK:
                    cpu->address_bus = 0xFFFE;
                    break;
                default:
                    UNREACHABLE();
            }
            cpu->t++;
            break;
        // Fetch adh
        case 5:
            cpu->internal_data = cpu->data_bus;
            cpu->address_bus++;
            cpu->t++;
            break;
        // Jump to interrupt vector
        case 6:
            cpu->pc = ((u16) cpu->data_bus << 8) | cpu->internal_data;
            cpu->address_bus = cpu->pc;
            cpu->addr_ready_t = 0;
            cpu->t = 0;
            cpu->interrupt_type = NONE;
            break;
        default:
            UNREACHABLE();
    }
}

void op_rti(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);

    switch (cpu->t) {
        case 1:
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        // Fetch p
        case 2:
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        // Fetch adl
        case 3:
            cpu->p = cpu->data_bus & ~FLAG_BREAK;
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        // Fetch adh
        case 4:
            cpu->internal_data = cpu->data_bus;
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 5:
            cpu->pc = ((u16) cpu->data_bus << 8) | cpu->internal_data;
            cpu->address_bus = cpu->pc;
            cpu->addr_ready_t = 0;
            cpu->t = 0;
            break;
        default:
            UNREACHABLE();
    }
}

void op_branch(CPU* cpu, Op op, u8 should_branch) {
    assert(op.addr_mode == ADDR_MODE_RELATIVE);

    // TODO: This feels really awkward. Figure out a nicer way to write it.
    switch (cpu->t) {
        case 1:
            if (!should_branch) {
                cpu_advance_pc(cpu);
                return;
            }

            cpu_advance_pc(cpu);

            u16 result = cpu->pc + (i8) cpu->data_bus;
            i16 start_page = cpu->pc >> 8;
            i16 end_page = result >> 8;
            // Page boundary crossed
            if (start_page != end_page) {
                i8 sign = get_bit(end_page - start_page, 7)*-2 + 1;
                // This is kind of disgusting, but if it works it works.
                cpu->internal_carry = sign;
                result -= 0x0100 * sign;
                cpu->pc = result;
                cpu->address_bus = cpu->pc;
                cpu->t++;
            } else {
                cpu->pc = result;
                cpu->address_bus = cpu->pc;
                cpu->t++;
            }
            break;
        case 2:
            if (cpu->internal_carry != 0) {
                cpu->pc += 0x0100 * cpu->internal_carry;
                cpu->address_bus = cpu->pc;
                cpu->t++;
            } else {
                cpu->pc--;
                cpu_advance(cpu);
            }
            break;
        case 3:
            cpu->pc--;
            cpu_advance(cpu);
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

    if (cpu->interrupt_type != NONE) {
        if (cpu->t == 0) {
            cpu->ir = 0x00;
            cpu->t++;
            return;
        }
    } else {
        if (cpu->t == 0) {
            assert(cpu->bus_mode == READ);
            cpu->ir = cpu->data_bus;
            cpu_advance_pc(cpu);
            cpu->t++;
            if (OPCODE_TABLE[cpu->ir].type == OP_BRK) {
                cpu->interrupt_type = BRK;
            }
            return;
        }
    }


    // TODO: Refactor a lot of common operations into their own function for
    // readability

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
            op_read_modify_write(cpu, op, rmw_inc);
            break;
        case OP_DEC:
            op_read_modify_write(cpu, op, rmw_dec);
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

        // Shift
        case OP_ASL:
            op_read_modify_write(cpu, op, rmw_lsr);
            break;
        case OP_LSR:
            op_read_modify_write(cpu, op, rmw_lsr);
            break;
        case OP_ROL:
            op_read_modify_write(cpu, op, rmw_rol);
            break;
        case OP_ROR:
            op_read_modify_write(cpu, op, rmw_ror);
            break;

        // Bitwise
        case OP_AND:
            op_and(cpu, op);
            break;
        case OP_ORA:
            op_ora(cpu, op);
            break;

        // Compare
        case OP_CMP:
            op_cmp(cpu, op, cpu->a);
            break;
        case OP_CPX:
            op_cmp(cpu, op, cpu->x);
            break;
        case OP_CPY:
            op_cmp(cpu, op, cpu->y);
            break;

        // Branch
        case OP_BCC:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_CARRY) == false);
            break;
        case OP_BCS:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_CARRY) == true);
            break;

        case OP_BEQ:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_ZERO) == true);
            break;
        case OP_BNE:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_ZERO) == false);
            break;

        case OP_BPL:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_NEGATIVE) == false);
            break;
        case OP_BMI:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_NEGATIVE) == true);
            break;

        case OP_BVC:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_OVERFLOW) == false);
            break;
        case OP_BVS:
            op_branch(cpu, op, cpu_get_status_flag(cpu, FLAG_OVERFLOW) == true);
            break;

        // Jump
        case OP_JMP:
            op_jmp(cpu, op);
            break;

        case OP_JSR:
            op_jsr(cpu, op);
            break;
        case OP_RTS:
            op_rts(cpu, op);
            break;

        case OP_BRK:
            op_brk(cpu, op);
            break;
        case OP_RTI:
            op_rti(cpu, op);
            break;

        // Other
        case OP_NOP:
            cpu->pc--;
            cpu_advance(cpu);
            break;

        case OP__UNDEFINED:
            exit(1);
        default:
            UNREACHABLE();
    }

    cpu->cycle++;
}
