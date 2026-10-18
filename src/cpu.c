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

// Signal instruction is done
static void cpu_instruction_done(CPU* cpu) {
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


    // NOTE: Delayed changes to I happen *AFTER* an IRQ is polled for the
    // instruction triggering the changing, not after the next instruction.
    switch (cpu->delay_interrupt_disable) {
        // Nothing
        case -1:
            break;
        case 0:
            cpu->p &= ~FLAG_INTERRUPT_DISABLE;
            cpu->delay_interrupt_disable = -1;
            break;
        case 1:
            cpu->p |= FLAG_INTERRUPT_DISABLE;
            cpu->delay_interrupt_disable = -1;
            break;
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
        '-', '_', // Expansion
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
        .pc = 0,
        .address_bus = 0,
        .s = 0,
        .p = FLAG_INTERRUPT_DISABLE | FLAG__EXPANSION,
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
            UNREACHABLE();
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

// =============================================================================
// Group One Instructions
// =============================================================================

typedef void (*GroupOneFunc)(CPU* cpu, void* ctx);

void op_group_one(CPU* cpu, Op op, GroupOneFunc func, void* ctx) {
    assert(cpu->t > 0);
    assert(cpu->bus_mode == READ);

    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        func(cpu, ctx);
        cpu_advance_pc(cpu);
        cpu_instruction_done(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, WRITE);
        return;
    }

    func(cpu, ctx);
    cpu_advance_pc(cpu);
    cpu_instruction_done(cpu);
}

void op_ld(CPU* cpu, void* ctx) {
    u8* reg = ctx;
    *reg = cpu->data_bus;
    cpu_set_zero_negative(cpu, *reg);
}

void op_adc(CPU* cpu, void* ctx) {
    (void) ctx;

    if (!cpu_get_status_flag(cpu, FLAG_DECIMAL)) {
        u16 result = cpu->a + cpu->data_bus + cpu_get_status_flag(cpu, FLAG_CARRY);

        u8 result_sign = get_bit(result, 7);
        u8 a_sign = get_bit(cpu->a, 7);
        u8 memory_sign = get_bit(cpu->data_bus, 7);
        b8 overflow = result_sign != a_sign && result_sign != memory_sign;

        cpu->a = result;

        cpu_set_status_flag(cpu, FLAG_CARRY, result > 0xFF);
        cpu_set_status_flag(cpu, FLAG_OVERFLOW, overflow);
        cpu_set_zero_negative(cpu, result);
    } else {
        u8 lhs_low  = cpu->a & 0x0F;
        u8 lhs_high = (cpu->a & 0xF0) >> 4;
        u8 rhs_low  = cpu->data_bus & 0x0F;
        u8 rhs_high = (cpu->data_bus & 0xF0) >> 4;

        u8 low_sum = lhs_low + rhs_low + cpu_get_status_flag(cpu, FLAG_CARRY);
        u8 high_sum = lhs_high + rhs_high;
        if (low_sum >= 10) {
            high_sum++;
            low_sum -= 10;
        }
        cpu_set_status_flag(cpu, FLAG_CARRY, high_sum >= 10);
        if (high_sum >= 10) {
            high_sum -= 10;
        }

        cpu->a = low_sum | (high_sum << 4);

        cpu_set_status_flag(cpu, FLAG_OVERFLOW, false);
        cpu_set_status_flag(cpu, FLAG_NEGATIVE, false);
        cpu_set_status_flag(cpu, FLAG_ZERO, cpu->a == 0);
    }
}

void op_sbc(CPU* cpu, void* ctx) {
    (void) ctx;

    if (!cpu_get_status_flag(cpu, FLAG_DECIMAL)) {
        i16 result = cpu->a + ~cpu->data_bus + cpu_get_status_flag(cpu, FLAG_CARRY);

        u8 result_sign = get_bit(result, 7);
        u8 a_sign = get_bit(cpu->a, 7);
        u8 memory_sign = get_bit(cpu->data_bus, 7);
        b8 overflow = result_sign != a_sign && result_sign == memory_sign;

        cpu->a = result & 0xFF;

        cpu_set_status_flag(cpu, FLAG_CARRY, result >= 0);
        cpu_set_status_flag(cpu, FLAG_OVERFLOW, overflow);
        cpu_set_zero_negative(cpu, result);
    } else {
        u8 lhs_low  = cpu->a & 0x0F;
        u8 lhs_high = cpu->a >> 4;

        u8 rhs_low  = cpu->data_bus & 0x0F;
        u8 rhs_high = cpu->data_bus >> 4;

        i8 diff_low = lhs_low - rhs_low - !cpu_get_status_flag(cpu, FLAG_CARRY);
        i8 diff_high = lhs_high - rhs_high;

        if (diff_low < 0) {
            diff_high--;
            diff_low += 10;
        }
        cpu_set_status_flag(cpu, FLAG_CARRY, diff_high >= 0);
        if (diff_high < 0) {
            diff_high += 10;
        }

        cpu->a = diff_low | (diff_high << 4);

        cpu_set_status_flag(cpu, FLAG_OVERFLOW, false);
        cpu_set_status_flag(cpu, FLAG_NEGATIVE, false);
        cpu_set_status_flag(cpu, FLAG_ZERO, cpu->a == 0);
    }
}

void op_and(CPU* cpu, void* ctx) {
    (void) ctx;
    cpu->a &= cpu->data_bus;
    cpu_set_zero_negative(cpu, cpu->a);
}

void op_ora(CPU* cpu, void* ctx) {
    (void) ctx;
    cpu->a |= cpu->data_bus;
    cpu_set_zero_negative(cpu, cpu->a);
}

void op_eor(CPU* cpu, void* ctx) {
    (void) ctx;
    cpu->a ^= cpu->data_bus;
    cpu_set_zero_negative(cpu, cpu->a);
}

void op_bit(CPU* cpu, void* ctx) {
    (void) ctx;
    u8 result = cpu->a & cpu->data_bus;
    cpu_set_status_flag(cpu, FLAG_ZERO, result == 0);
    cpu_set_status_flag(cpu, FLAG_OVERFLOW, get_bit(cpu->data_bus, 6));
    cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(cpu->data_bus, 7));
}

void op_cmp(CPU* cpu, void* ctx) {
    u8* reg = ctx;
    cpu_set_status_flag(cpu, FLAG_CARRY, *reg >= cpu->data_bus);
    cpu_set_status_flag(cpu, FLAG_ZERO, *reg == cpu->data_bus);
    cpu_set_status_flag(cpu, FLAG_NEGATIVE, get_bit(*reg - cpu->data_bus, 7));
}

// =============================================================================
// Read-Modify-Write Instructions
// =============================================================================

typedef void (*RmwFunc)(CPU* cpu, u8* memory);

void op_read_modify_write(CPU* cpu, Op op, RmwFunc func) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        func(cpu, &cpu->a);
        cpu_instruction_done(cpu);
        return;
    }

    if (!cpu_is_addr_ready(cpu)) {
        step_addressing_mode(cpu, op.addr_mode, WRITE);
        return;
    }

    if (cpu->t == cpu->addr_ready_t + 1) {
        // cpu->data_bus = cpu->data_bus;
        cpu->bus_mode = WRITE;
        cpu->t++;
    } else if (cpu->t == cpu->addr_ready_t + 2) {
        cpu->bus_mode = WRITE;
        func(cpu, &cpu->data_bus);
        cpu->t++;
    } else if (cpu->t == cpu->addr_ready_t + 3) {
        cpu_advance_pc(cpu);
        cpu_instruction_done(cpu);
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

// =============================================================================
// Uncategorized Instructions
// =============================================================================

void op_inc_register(CPU* cpu, Op op, u8* reg) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *reg += 1;
    cpu_set_zero_negative(cpu, *reg);

    cpu_instruction_done(cpu);
}

void op_dec_register(CPU* cpu, Op op, u8* reg) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *reg -= 1;
    cpu_set_zero_negative(cpu, *reg);

    cpu_instruction_done(cpu);
}

void op_st(CPU* cpu, Op op, u8* reg) {
    assert(cpu->t > 0);

    // Addressing is done, instruction is done.
    if (cpu_is_addr_ready(cpu)) {
        cpu_advance_pc(cpu);
        cpu_instruction_done(cpu);
        return;
    }

    step_addressing_mode(cpu, op.addr_mode, WRITE);

    if (cpu_is_addr_ready(cpu)) {
        cpu->data_bus = *reg;
        cpu->bus_mode = WRITE;
    }
}

void op_transfer(CPU* cpu, Op op, u8 left, u8* right, b8 set_flags) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    *right = left;

    if (set_flags) {
        cpu_set_zero_negative(cpu, *right);
    }

    cpu_instruction_done(cpu);
}

void op_jmp(CPU* cpu, Op op) {
    step_addressing_mode(cpu, op.addr_mode, READ);
    if (cpu_is_addr_ready(cpu)) {
        u16 address = cpu->address_bus;
        cpu->pc = address;
        cpu_instruction_done(cpu);
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
            cpu->pc = ((u16) cpu->data_bus << 8) | cpu->internal_data;
            cpu_instruction_done(cpu);
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
            cpu_advance_pc(cpu);
            cpu_instruction_done(cpu);
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
            cpu->delay_interrupt_disable = -1;
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
                cpu_instruction_done(cpu);
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
                cpu_instruction_done(cpu);
            }
            break;
        case 3:
            cpu_instruction_done(cpu);
            break;
    }
}

void op_push(CPU* cpu, Op op, u8 value) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);

    switch (cpu->t) {
        case 1:
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->s--;
            cpu->data_bus = value;
            cpu->bus_mode = WRITE;
            cpu->t++;
            break;
        case 2:
            cpu_instruction_done(cpu);
            break;
    }
}

i16 op_pull(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);

    switch (cpu->t) {
        case 1:
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 2:
            cpu->s++;
            cpu->address_bus = 0x0100 | cpu->s;
            cpu->t++;
            break;
        case 3:
            cpu_instruction_done(cpu);
            return cpu->data_bus;
    }

    return -1;
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
            op_group_one(cpu, op, op_ld, &cpu->a);
            break;
        case OP_STA:
            op_st(cpu, op, &cpu->a);
            break;

        case OP_LDX:
            op_group_one(cpu, op, op_ld, &cpu->x);
            break;
        case OP_STX:
            op_st(cpu, op, &cpu->x);
            break;

        case OP_LDY:
            op_group_one(cpu, op, op_ld, &cpu->y);
            break;
        case OP_STY:
            op_st(cpu, op, &cpu->y);
            break;

        // Transfer
        case OP_TAX:
            op_transfer(cpu, op, cpu->a, &cpu->x, true);
            break;
        case OP_TXA:
            op_transfer(cpu, op, cpu->x, &cpu->a, true);
            break;

        case OP_TAY:
            op_transfer(cpu, op, cpu->a, &cpu->y, true);
            break;
        case OP_TYA:
            op_transfer(cpu, op, cpu->y, &cpu->a, true);
            break;

        // Arithmetic
        case OP_ADC:
            op_group_one(cpu, op, op_adc, NULL);
            break;
        case OP_SBC:
            op_group_one(cpu, op, op_sbc, NULL);
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
            op_read_modify_write(cpu, op, rmw_asl);
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
            op_group_one(cpu, op, op_and, NULL);
            break;
        case OP_ORA:
            op_group_one(cpu, op, op_ora, NULL);
            break;
        case OP_EOR:
            op_group_one(cpu, op, op_eor, NULL);
            break;
        case OP_BIT:
            op_group_one(cpu, op, op_bit, NULL);
            break;

        // Compare
        case OP_CMP:
            op_group_one(cpu, op, op_cmp, &cpu->a);
            break;
        case OP_CPX:
            op_group_one(cpu, op, op_cmp, &cpu->x);
            break;
        case OP_CPY:
            op_group_one(cpu, op, op_cmp, &cpu->y);
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

        // Stack
        case OP_PHA:
            op_push(cpu, op, cpu->a);
            break;
        case OP_PLA:
            cpu->a = op_pull(cpu, op);
            cpu_set_zero_negative(cpu, cpu->a);
            break;

        case OP_PHP:
            op_push(cpu, op, cpu->p | FLAG_BREAK);
            break;
        case OP_PLP: {
            i16 result = op_pull(cpu, op);
            if (result != -1) {
                cpu->delay_interrupt_disable = (result & FLAG_INTERRUPT_DISABLE) != 0;
                // Clear I
                result &= ~FLAG_INTERRUPT_DISABLE;
                // Clear B
                result &= ~FLAG_BREAK;
                // Take the value of current I
                result |= (cpu->p & FLAG_INTERRUPT_DISABLE);
                // Expansion is ALWAYS on
                cpu->p = result | FLAG__EXPANSION;
                cpu_instruction_done(cpu);
            }
        } break;

        case OP_TXS:
            op_transfer(cpu, op, cpu->x, &cpu->s, false);
            break;
        case OP_TSX:
            op_transfer(cpu, op, cpu->s, &cpu->x, true);
            break;

        // Flags
        case OP_CLC:
            cpu->p &= ~FLAG_CARRY;
            cpu_instruction_done(cpu);
            break;
        case OP_SEC:
            cpu->p |= FLAG_CARRY;
            cpu_instruction_done(cpu);
            break;

        case OP_CLI:
            cpu->delay_interrupt_disable = 0;
            cpu_instruction_done(cpu);
            break;
        case OP_SEI:
            cpu->delay_interrupt_disable = 1;
            cpu_instruction_done(cpu);
            break;

        case OP_CLD:
            cpu->p &= ~FLAG_DECIMAL;
            cpu_instruction_done(cpu);
            break;
        case OP_SED:
            cpu->p |= FLAG_DECIMAL;
            cpu_instruction_done(cpu);
            break;

        case OP_CLV:
            cpu->p &= ~FLAG_OVERFLOW;
            cpu_instruction_done(cpu);
            break;

        // Other
        case OP_NOP:
            cpu_instruction_done(cpu);
            break;

        case OP__UNDEFINED:
            exit(1);
        default:
            fprintf(stderr, "ERR: Unknown opcode: 0x%02X\n", cpu->ir);
            UNREACHABLE();
    }
    assert(cpu_get_status_flag(cpu, FLAG__EXPANSION) == 1);
    assert(cpu_get_status_flag(cpu, FLAG_BREAK) == 0);

    cpu->cycle++;
}
