#ifndef CPU_H
#define CPU_H

#include "nes/common.h"

enum {
    CPU_STATUS_NONE,

    CPU_STATUS_CARRY                = 0x01,
    CPU_STATUS_ZERO                 = 0x02,
    CPU_STATUS_INTERRUPT_DISABLE    = 0x04,
    CPU_STATUS__UNUSED              = 0x08,
    CPU_STATUS_DECIMAL              = 0x10,
    CPU_STATUS_BREAK                = 0x20,
    CPU_STATUS_OVERFLOW             = 0x40,
    CPU_STATUS_NEGATIVE             = 0x80,

    CPU_STATUS__COUNT,
};

// static const char* CPU_STATUS_STRING[CPU_STATUS__COUNT] = {
//     "ERR: None",
//     "negative",
//     "overflow",
//     "break",
//     "decimal",
//     "ERR: Unused",
//     "interrupt disable",
//     "zero",
//     "carry",
// };

typedef struct CPU CPU;
struct CPU {
    // A register
    u8 accumulator;
    // X-index register
    u8 x;
    // Y-index register
    u8 y;
    // S register
    u8 stack_pointer;
    // PC register
    u16 program_counter;
    // P register
    u8 status;
};

#define STACK_BEGIN 0x0100
#define STACK_END   0x01FF

// [Reference](https://www.nesdev.org/wiki/Instruction_reference#BRK)
typedef u8 OpType;
enum {
    // Access
    OP_LDA, OP_STA,
    OP_LDX, OP_STX,
    OP_LDY, OP_STY,
    // Transfer
    OP_TAX, OP_TXA,
    OP_TAY, OP_TYA,
    // Arithmetic
    OP_ADC, OP_SBC,
    OP_INC, OP_DEC,
    OP_DEX, OP_INX,
    OP_INY, OP_DEY,
    // Shift
    OP_ASL, OP_LSR,
    OP_ROL, OP_ROR,
    // Bitwise
    OP_AND, OP_ORA,
    OP_EOR, OP_BIT,
    // Compare
    OP_CMP, OP_CPX,
    OP_CPY,
    // Branch
    OP_BCC, OP_BCS,
    OP_BEQ, OP_BNE,
    OP_BPL, OP_BMI,
    OP_BVC, OP_BVS,
    // Jump
    OP_JMP, OP_JSR,
    OP_RTS, OP_BRK,
    OP_RTI,
    // Stack
    OP_PHA, OP_PLA,
    OP_PHP, OP_PLP,
    OP_TXS, OP_TSX,
    // Flags
    OP_CLC, OP_SEC,
    OP_CLI, OP_SEI,
    OP_CLD, OP_SED, OP_CLV,
    OP_NOP,

    OP__COUNT,
    OP__UNDEFINED = 0xFF,
};

// [Reference](https://www.nesdev.org/wiki/CPU_addressing_modes)
typedef u8 AddrMode;
enum {
    ADDR_MODE_ACCUMULATOR,
    ADDR_MODE_IMMEDIATE,
    ADDR_MODE_ZERO_PAGE,
    ADDR_MODE_ZERO_PAGE_X,
    ADDR_MODE_ABSOLUTE,
    ADDR_MODE_ABSOLUTE_X,
    ADDR_MODE_ABSOLUTE_Y,
    ADDR_MODE_INDIRECT,
    ADDR_MODE_INDIRECT_X,
    ADDR_MODE_INDIRECT_Y,
    ADDR_MODE_RELATIVE,

    ADDR_MODE__COUNT,
    ADDR_MODE__UNDEFINED = 0xFF,
};

typedef struct Op Op;
struct Op {
    OpType type;
    AddrMode addr_mode;
};

#endif // CPU_H
