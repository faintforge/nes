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
    cpu_set_status_flag(cpu, CPU_STATUS_ZERO, value == 0);
    cpu_set_status_flag(cpu, CPU_STATUS_NEGATIVE, get_bit(value, 7));
}

// =============================================================================
// Memory operation helpers 
// =============================================================================

static u8 cpu_read(CPU* cpu, u16 address) {
    cpu->cycle++;
    return cpu->bus.read(&cpu->bus, address);
}

static void cpu_write(CPU* cpu, u16 address, u8 value) {
    cpu->cycle++;
    cpu->bus.write(&cpu->bus, address, value);
}

static u8 cpu_fetch(CPU* cpu) {
    u8 data = cpu_read(cpu, cpu->pc);
    cpu->pc++;
    return data;
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
        .p = CPU_STATUS_INTERRUPT_DISABLE,
    };

    return cpu;
}

void cpu_reset(CPU* cpu) {
    cpu->pc = 0xFFFC;
    cpu_set_status_flag(cpu, CPU_STATUS_INTERRUPT_DISABLE, true);

    // TODO: Make cpu reset and actual interrupt.

    // We pop three times because this is a specialiced interrupt.
    for (u8 i = 0; i < 3; i++) {
        stack_pop(cpu);
    }

    u8 pc_low = cpu_fetch(cpu);
    u8 pc_high = cpu_fetch(cpu);
    cpu->pc = (u16) (pc_high << 8) | pc_low;
}

static Op opcode_decode(u8 opcode) {
    return OPCODE_TABLE[opcode];
}

static u16 get_address(CPU* cpu, AddrMode mode, b8 always_oops) {
    // Implementation details can be found in chapter 6 of the programmers manual
    // for the 6502.

    switch (mode) {
        case ADDR_MODE_ACCUMULATOR:
            break;
        case ADDR_MODE_IMMEDIATE:
            break;
        case ADDR_MODE_ZERO_PAGE:
            return cpu_fetch(cpu);
        case ADDR_MODE_ZERO_PAGE_X:
            // Oops cycle
            cpu->cycle++;
            return cpu_fetch(cpu) + cpu->x;
        case ADDR_MODE_ZERO_PAGE_Y:
            // Oops cycle
            cpu->cycle++;
            return cpu_fetch(cpu) + cpu->y;
        case ADDR_MODE_ABSOLUTE: {
            u8 low = cpu_fetch(cpu);
            u16 high = cpu_fetch(cpu);
            return (high << 8) | low;
         }
        case ADDR_MODE_ABSOLUTE_X: {
            u8 low = cpu_fetch(cpu);
            u16 high = cpu_fetch(cpu);
            // Oops cycle
            if (always_oops || (u16) low + cpu->x > 0xFF) {
                cpu->cycle++;
            }
            return ((high << 8) | low) + cpu->x;
         }
        case ADDR_MODE_ABSOLUTE_Y: {
            u8 low = cpu_fetch(cpu);
            u16 high = cpu_fetch(cpu);
            // Oops cycle
            if (always_oops || (u16) low + cpu->y > 0xFF) {
                cpu->cycle++;
            }
            return ((high << 8) | low) + cpu->y;
        }
        case ADDR_MODE_INDIRECT: {
            u8 zero_page_addr = cpu_fetch(cpu);
            u8 low = cpu_read(cpu, zero_page_addr);
            u16 high = cpu_read(cpu, zero_page_addr+1);
            return ((high << 8) | low);
        }
        case ADDR_MODE_INDIRECT_X: {
            u8 zero_page_addr = cpu_fetch(cpu);
            u8 low = cpu_read(cpu, zero_page_addr + cpu->x);
            u16 high = cpu_read(cpu, zero_page_addr + cpu->x + 1);
            // Always add and oops cycle because otherwise the next instruction
            // could read the wrong address on the wrong page while the ALU
            // fixes the page overflow.
            cpu->cycle++;
            return ((high << 8) | low);
        }
        case ADDR_MODE_INDIRECT_Y: {
            u8 zero_page_addr = cpu_fetch(cpu);
            u8 low = cpu_read(cpu, zero_page_addr);
            u16 high = cpu_read(cpu, zero_page_addr + 1);
            // Oops cycle
            if (always_oops || (u16) low + cpu->y > 0xFF) {
                cpu->cycle++;
            }
            return ((high << 8) | low) + cpu->y;
        }
        case ADDR_MODE_RELATIVE: {
            i8 offset = cpu_fetch(cpu);
            return cpu->pc + offset;
        }
        case ADDR_MODE_IMPLIED:
            break;
    }

    fprintf(stderr, "ERR: %s(): Addressing mode %s not allowed.\n", __func__, addr_mode_enum_string(mode));
    exit(1);
}

static void op_ld(CPU* cpu, Op op, u8* reg) {
    assert(reg != NULL);
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        *reg = cpu_fetch(cpu);
    } else {
        u16 addr = get_address(cpu, op.addr_mode, false);
        *reg = cpu_read(cpu, addr);
    }

    cpu_set_zero_negative(cpu, *reg);
}

static void op_st(CPU* cpu, Op op, u8* reg) {
    assert(reg != NULL);
    u16 addr = get_address(cpu, op.addr_mode, true);
    cpu_write(cpu, addr, *reg);
}

static void op_adc(CPU* cpu, Op op) {
    u8 memory = -1;
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        memory = cpu_fetch(cpu);
    } else {
        u16 addr = get_address(cpu, op.addr_mode, false);
        memory = cpu_read(cpu, addr);
    }
    u16 result = (u16) cpu->a + memory + cpu_get_status_flag(cpu, CPU_STATUS_CARRY);

    // Taken straight from nesdev.org
    b8 overflow = ((result^cpu->a) & (result^memory) & 0x80) != 0;

    cpu_set_status_flag(cpu, CPU_STATUS_CARRY, result > 0xFF);
    cpu_set_status_flag(cpu, CPU_STATUS_OVERFLOW, overflow);
    cpu_set_zero_negative(cpu, result);

    cpu->a = result;
}

static void op_sbc(CPU* cpu, Op op) {
    u8 memory = -1;
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        memory = cpu_fetch(cpu);
    } else {
        u16 addr = get_address(cpu, op.addr_mode, false);
        memory = cpu_read(cpu, addr);
    }
    // www.nesdev.org/wiki/Instruction_reference#SBC
    u8 result = cpu->a - memory - cpu_get_status_flag(cpu, CPU_STATUS_CARRY);
    b8 overflow = ((result^cpu->a) & (result^memory) & 0x80) != 0;

    cpu_set_status_flag(cpu, CPU_STATUS_CARRY, (i8) result < 0x00);
    cpu_set_status_flag(cpu, CPU_STATUS_OVERFLOW, overflow);
    cpu_set_zero_negative(cpu, result);

    cpu->a = result;
}

static void op_inc(CPU* cpu, Op op) {
    u16 addr = get_address(cpu, op.addr_mode, true);
    u8 memory = cpu_read(cpu, addr);
    // Don't know why it does this extra write, but it does.
    cpu_write(cpu, addr, memory);
    memory++;
    cpu_write(cpu, addr, memory);

    cpu_set_zero_negative(cpu, memory);
}

static void op_dec(CPU* cpu, Op op) {
    u16 addr = get_address(cpu, op.addr_mode, true);
    u8 memory = cpu_read(cpu, addr);
    // Don't know why it does this extra write, but it does.
    cpu_write(cpu, addr, memory);
    memory--;
    cpu_write(cpu, addr, memory);

    cpu_set_zero_negative(cpu, memory);
}

static void op_asl(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        b8 carry = get_bit(cpu->a, 7);
        cpu->a <<= 1;
        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, carry);
        cpu_set_zero_negative(cpu, cpu->a);

        cpu->cycle++;
    } else {
        u16 addr = get_address(cpu, op.addr_mode, true);
        u8 value = cpu_read(cpu, addr);
        b8 carry = get_bit(value, 7);

        cpu_write(cpu, addr, value);
        value <<= 1;
        cpu_write(cpu, addr, value);

        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, carry);
        cpu_set_zero_negative(cpu, value);
    }
}

static void op_lsr(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        b8 carry = get_bit(cpu->a, 0);
        cpu->a >>= 1;
        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, carry);
        cpu_set_zero_negative(cpu, cpu->a);

        cpu->cycle++;
    } else {
        u16 addr = get_address(cpu, op.addr_mode, true);
        u8 value = cpu_read(cpu, addr);
        b8 carry = get_bit(value, 0);

        cpu_write(cpu, addr, value);
        value >>= 1;
        cpu_write(cpu, addr, value);

        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, carry);
        cpu_set_zero_negative(cpu, value);
    }
}

static void op_rol(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        b8 new_carry = get_bit(cpu->a, 7);
        b8 old_carry = cpu_get_status_flag(cpu, CPU_STATUS_CARRY);
        cpu->a = (cpu->a << 1) | old_carry;
        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, new_carry);
        cpu_set_zero_negative(cpu, cpu->a);

        cpu->cycle++;
    } else {
        u16 addr = get_address(cpu, op.addr_mode, true);
        u8 value = cpu_read(cpu, addr);

        b8 old_carry = cpu_get_status_flag(cpu, CPU_STATUS_CARRY);
        u8 result = (value << 1) | old_carry;

        cpu_write(cpu, addr, value);
        cpu_write(cpu, addr, result);

        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, get_bit(value, 7));
        cpu_set_zero_negative(cpu, result);
    }
}

static void op_ror(CPU* cpu, Op op) {
    if (op.addr_mode == ADDR_MODE_ACCUMULATOR) {
        b8 new_carry = get_bit(cpu->a, 0);
        b8 old_carry = cpu_get_status_flag(cpu, CPU_STATUS_CARRY);
        cpu->a = (cpu->a >> 1) | (old_carry << 7);
        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, new_carry);
        cpu_set_zero_negative(cpu, cpu->a);

        cpu->cycle++;
    } else {
        u16 addr = get_address(cpu, op.addr_mode, true);
        u8 value = cpu_read(cpu, addr);

        b8 old_carry = cpu_get_status_flag(cpu, CPU_STATUS_CARRY);
        u8 result = (value >> 1) | (old_carry << 7);

        cpu_write(cpu, addr, value);
        cpu_write(cpu, addr, result);

        cpu_set_status_flag(cpu, CPU_STATUS_CARRY, get_bit(value, 0));
        cpu_set_zero_negative(cpu, result);
    }
}

static void op_jsr(CPU* cpu, Op op) {
    u16 addr = get_address(cpu, op.addr_mode, false);

    // Perform an extra cycle to put the low byte of the new address onto the
    // address bus.
    cpu->cycle++;

    // Make sure to push *after* reading the next two bytes.
    // Subtract one because RTS increments pc by 1.
    u8 pc_high = (cpu->pc - 1) >> 8;
    u8 pc_low = (cpu->pc - 1) & 0xFF;
    stack_push(cpu, pc_high);
    stack_push(cpu, pc_low);

    cpu->pc = addr;
}

static void op_brk(CPU* cpu, Op op) {
    (void) op;

    u8 pc_high = (cpu->pc + 1) >> 8;
    u8 pc_low = (cpu->pc + 1) & 0xFF;
    stack_push(cpu, pc_high);
    stack_push(cpu, pc_low);
    stack_push(cpu, cpu->p | CPU_STATUS_BREAK | CPU_STATUS__EXPANSION);

    // Interrupt vector
    u8 handler_addr_low = cpu_read(cpu, 0xFFFE);
    u8 handler_addr_high = cpu_read(cpu, 0xFFFF);
    cpu->pc = ((u16) handler_addr_high << 8) | (handler_addr_low);

    cpu_set_status_flag(cpu, CPU_STATUS_INTERRUPT_DISABLE, true);
}

// Implied addressing always incur an extra cycle.
static inline void implied_addressing(CPU* cpu, Op op) {
    assert(op.addr_mode == ADDR_MODE_IMPLIED);
    cpu->cycle++;
}

static void cpu_execute(CPU* cpu, Op op, u8 opcode) {
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
            implied_addressing(cpu, op);
            cpu->x = cpu->a;
            cpu_set_zero_negative(cpu, cpu->x);
            break;
        case OP_TXA:
            implied_addressing(cpu, op);
            cpu->a = cpu->x;
            cpu_set_zero_negative(cpu, cpu->a);
            break;
        case OP_TAY:
            implied_addressing(cpu, op);
            cpu->y = cpu->a;
            cpu_set_zero_negative(cpu, cpu->y);
            break;
        case OP_TYA:
            implied_addressing(cpu, op);
            cpu->a = cpu->y;
            cpu_set_zero_negative(cpu, cpu->a);
            break;

        // Shift
        case OP_ASL:
            op_asl(cpu, op);
            break;
        case OP_LSR:
            op_lsr(cpu, op);
            break;
        case OP_ROL:
            op_rol(cpu, op);
            break;
        case OP_ROR:
            op_ror(cpu, op);
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
            implied_addressing(cpu, op);
            cpu->x++;
            cpu_set_zero_negative(cpu, cpu->x);
            break;
        case OP_DEX:
            implied_addressing(cpu, op);
            cpu->x--;
            cpu_set_zero_negative(cpu, cpu->x);
            break;
        case OP_INY:
            implied_addressing(cpu, op);
            cpu->y++;
            cpu_set_zero_negative(cpu, cpu->y);
            break;
        case OP_DEY:
            implied_addressing(cpu, op);
            cpu->y--;
            cpu_set_zero_negative(cpu, cpu->y);
            break;

        // Jump
        case OP_JMP: {
            u16 addr = get_address(cpu, op.addr_mode, false);
            cpu->pc = addr;
        } break;
        case OP_JSR:
            op_jsr(cpu, op);
            break;
        case OP_RTS: {
            implied_addressing(cpu, op);
            u8 addr_low = stack_pop(cpu);
            u8 addr_high = stack_pop(cpu);
            u16 addr = (addr_high << 8) | addr_low;
            cpu->pc = addr + 1;
            // Add an extra two cycles because of the parallel fetch decode
            // shenanigans the 6502 does.
            cpu->cycle += 2;
        } break;
        case OP_BRK:
            implied_addressing(cpu, op);
            op_brk(cpu, op);
            break;
        case OP_RTI: {
            implied_addressing(cpu, op);
            u8 status = stack_pop(cpu);
            status &= ~(CPU_STATUS_BREAK | CPU_STATUS__EXPANSION);
            cpu->p = status;

            // Yet again another cycle because the parallel fetch and decode
            // behavior.
            cpu->cycle++;

            u8 addr_low = stack_pop(cpu);
            u8 addr_high = stack_pop(cpu);
            u16 addr = (addr_high << 8) | addr_low;
            cpu->pc = addr;
        } break;

        // Stack
        case OP_PHA:
            implied_addressing(cpu, op);
            stack_push(cpu, cpu->a);
            cpu->s--;
            break;
        case OP_PLA:
            implied_addressing(cpu, op);
            cpu->a = stack_pop(cpu);
            cpu_set_zero_negative(cpu, cpu->a);
            break;
        case OP_PHP:
            implied_addressing(cpu, op);
            stack_push(cpu, cpu->p | CPU_STATUS_BREAK | CPU_STATUS__EXPANSION);
            break;
        case OP_PLP: {
            implied_addressing(cpu, op);
            u8 stack_p = stack_pop(cpu);
            u8 old_interrupt_flag = cpu->p & CPU_STATUS_INTERRUPT_DISABLE;
            cpu->p = (stack_p & ~CPU_STATUS_INTERRUPT_DISABLE) | old_interrupt_flag;
            // TODO: Delay setting interrupt flag by one cycle because of
            // interrupt polling.
        } break;
        case OP_TXS:
            implied_addressing(cpu, op);
            cpu->s = cpu->x;
            break;
        case OP_TSX:
            implied_addressing(cpu, op);
            cpu->x = cpu->s;
            break;

        // Flags
        case OP_CLC:
            implied_addressing(cpu, op);
            cpu->p &= ~CPU_STATUS_CARRY;
            break;
        case OP_SEC:
            implied_addressing(cpu, op);
            cpu->p |= CPU_STATUS_CARRY;
            break;
        case OP_CLI:
            implied_addressing(cpu, op);
            cpu->p &= ~CPU_STATUS_INTERRUPT_DISABLE;
            break;
        case OP_SEI:
            implied_addressing(cpu, op);
            cpu->p |= CPU_STATUS_INTERRUPT_DISABLE;
            break;
        case OP_CLD:
            implied_addressing(cpu, op);
            cpu->p &= ~CPU_STATUS_DECIMAL;
            break;
        case OP_SED:
            implied_addressing(cpu, op);
            cpu->p |= CPU_STATUS_DECIMAL;
            break;
        case OP_CLV:
            implied_addressing(cpu, op);
            cpu->p &= ~CPU_STATUS_OVERFLOW;
            break;

        // Other
        case OP_NOP:
            implied_addressing(cpu, op);
            break;

        case OP__UNDEFINED:
            printf("ERR: Undefined instruction : %02X\n", opcode);
            exit(1);
        default:
            printf("ERR: Unimplemented instruction: %02X\n", opcode);
            exit(1);
    }
}

u8 cpu_step(CPU* cpu) {
    u64 start_cycle = cpu->cycle;
    u8 opcode = cpu_fetch(cpu);
    Op op = opcode_decode(opcode);;
    cpu_execute(cpu, op, opcode);
    return cpu->cycle - start_cycle;
}
