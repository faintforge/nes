#ifndef OP_H
#define OP_H

#include "nes/common.h"

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
    ADDR_MODE_ZERO_PAGE_Y,
    ADDR_MODE_ABSOLUTE,
    ADDR_MODE_ABSOLUTE_X,
    ADDR_MODE_ABSOLUTE_Y,
    ADDR_MODE_INDIRECT,
    ADDR_MODE_INDIRECT_X,
    ADDR_MODE_INDIRECT_Y,
    ADDR_MODE_RELATIVE,
    ADDR_MODE_IMPLIED,

    ADDR_MODE__COUNT,
    ADDR_MODE__UNDEFINED = 0xFF,
};

typedef struct Op Op;
struct Op {
    OpType type;
    AddrMode addr_mode;
};

static const Op OP_UNDEFINED = {
    .type = OP__UNDEFINED,
    .addr_mode = ADDR_MODE__UNDEFINED,
};

extern const char* op_pretty_string(OpType type);
extern const char* addr_mode_pretty_string(AddrMode mode);
extern const char* op_enum_string(OpType type);
extern const char* addr_mode_enum_string(AddrMode mode);

#endif // OP_H
