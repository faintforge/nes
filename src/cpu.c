#include "nes/cpu.h"
#include "nes/opcode_table.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define MEMORY_SIZE 0x10000

void print_cpu_status(u8 status) {
    printf("CPU Status:\n");
    printf("    C = %d\n", (status >> 0) & 1);
    printf("    Z = %d\n", (status >> 1) & 1);
    printf("    I = %d\n", (status >> 2) & 1);
    printf("    D = %d\n", (status >> 3) & 1);
    printf("    B = %d\n", (status >> 4) & 1);
    printf("    O = %d\n", (status >> 6) & 1);
    printf("    Z = %d\n", (status >> 7) & 1);
}

CPU cpu_create(void) {
    // [Power up state](https://www.nesdev.org/wiki/CPU_power_up_state)
    // [Memory map](https://www.nesdev.org/wiki/CPU_memory_map)
    CPU cpu = {
        .memory = malloc(MEMORY_SIZE),
        .a = 0,
        .x = 0,
        .y = 0,
        .pc = 0xFFFC,
        .s = 0xFD,
        .p = CPU_STATUS_INTERRUPT_DISABLE,
    };
    memset(cpu.memory, 0, MEMORY_SIZE);

    return cpu;
}

void cpu_destroy(CPU* cpu) {
    free(cpu->memory);
    *cpu = (CPU) {0};
}

void cpu_reset(CPU* cpu) {
    cpu->pc = 0xFFFC;
    cpu->s -= 3;
    cpu->p |= CPU_STATUS_INTERRUPT_DISABLE;
}

static u8 cpu_read(CPU* cpu, u16 address) {
    u8 data = cpu->memory[address];
    cpu->cycle++;
    return data;
}

static void cpu_write(CPU* cpu, u16 address, u8 value) {
    cpu->memory[address] = value;
    cpu->cycle++;
}

static u8 cpu_fetch(CPU* cpu) {
    u8 data = cpu->memory[cpu->pc];
    cpu->cycle++;
    cpu->pc++;
    return data;
}

static Op opcode_decode(u8 opcode) {
    return OPCODE_TABLE[opcode];
}

static u32 get_address(CPU* cpu, AddrMode mode, b8 always_oops) {
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
            // TODO: Figure out how negative numbers work in this and how that
            // affects relative offsets.
            i8 offset = cpu_fetch(cpu);
            return cpu->pc + offset;
        }
        case ADDR_MODE_IMPLIED:
            break;
    }

    assert(false && "Unreachable");
    return ~0u;
}

static void op_ld(CPU* cpu, Op op, u8* reg) {
    assert(reg != NULL);
    if (op.addr_mode == ADDR_MODE_IMMEDIATE) {
        *reg = cpu_fetch(cpu);
    } else {
        u32 addr = get_address(cpu, op.addr_mode, false);
        if (addr > 0xFFFF) {
            exit(1);
        }
        *reg = cpu_read(cpu, addr);
    }

    if (cpu->a == 0) {
        cpu->p |= CPU_STATUS_ZERO;
    } else {
        cpu->p &= ~CPU_STATUS_ZERO;
    }

    if (((cpu->a >> 6) & 1) == 1) {
        cpu->p |= CPU_STATUS_NEGATIVE;
    } else {
        cpu->p &= ~CPU_STATUS_NEGATIVE;
    }
}

static void op_st(CPU* cpu, Op op, u8* reg) {
    assert(reg != NULL);
    u32 addr = get_address(cpu, op.addr_mode, true);
    if (addr > 0xFFFF) {
        exit(1);
    }
    cpu_write(cpu, addr, *reg);
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
            break;
        case OP_TXA:
            implied_addressing(cpu, op);
            cpu->a = cpu->x;
            break;
        case OP_TAY:
            implied_addressing(cpu, op);
            cpu->y = cpu->a;
            break;
        case OP_TYA:
            implied_addressing(cpu, op);
            cpu->a = cpu->y;
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

void cpu_step(CPU* cpu) {
    u8 opcode = cpu_fetch(cpu);
    Op op = opcode_decode(opcode);;
    cpu_execute(cpu, op, opcode);
}
