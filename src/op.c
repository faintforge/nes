#include "nes/op.h"

const char* op_pretty_string(OpType type) {
    static const char* OP_PRETTY_STRING[OP__COUNT] = {
        "LDA", "STA", "LDX", "STX", "LDY", "STY", "TAX", "TXA", "TAY", "TYA",
        "ADC", "SBC", "INC", "DEC", "DEX", "INX", "INY", "DEY", "ASL", "LSR",
        "ROL", "ROR", "AND", "ORA", "EOR", "BIT", "CMP", "CPX", "CPY", "BCC",
        "BCS", "BEQ", "BNE", "BPL", "BMI", "BVC", "BVS", "JMP", "JSR", "RTS",
        "BRK", "RTI", "PHA", "PLA", "PHP", "PLP", "TXS", "TSX", "CLC", "SEC",
        "CLI", "SEI", "CLD", "SED", "CLV", "NOP",
    };
    return OP_PRETTY_STRING[type];
}

const char* addr_mode_pretty_string(AddrMode mode) {
    static const char* ADDR_MODE_PRETTY_STRING[ADDR_MODE__COUNT] = {
        "A",     "#imm", "zpg",   "zpg,X", "zpg,Y", "abs", "abs,X",
        "abs,Y", "(ind)",  "(ind,X)", "(ind),Y", "rel",   "",
    };
    return ADDR_MODE_PRETTY_STRING[mode];
}

const char* op_enum_string(OpType type) {
    static const char* OP_ENUM_STRING[OP__COUNT] = {
        "OP_LDA", "OP_STA", "OP_LDX", "OP_STX", "OP_LDY", "OP_STY", "OP_TAX",
        "OP_TXA", "OP_TAY", "OP_TYA", "OP_ADC", "OP_SBC", "OP_INC", "OP_DEC",
        "OP_DEX", "OP_INX", "OP_INY", "OP_DEY", "OP_ASL", "OP_LSR", "OP_ROL",
        "OP_ROR", "OP_AND", "OP_ORA", "OP_EOR", "OP_BIT", "OP_CMP", "OP_CPX",
        "OP_CPY", "OP_BCC", "OP_BCS", "OP_BEQ", "OP_BNE", "OP_BPL", "OP_BMI",
        "OP_BVC", "OP_BVS", "OP_JMP", "OP_JSR", "OP_RTS", "OP_BRK", "OP_RTI",
        "OP_PHA", "OP_PLA", "OP_PHP", "OP_PLP", "OP_TXS", "OP_TSX", "OP_CLC",
        "OP_SEC", "OP_CLI", "OP_SEI", "OP_CLD", "OP_SED", "OP_CLV", "OP_NOP",
    };
    return OP_ENUM_STRING[type];
}

const char* addr_mode_enum_string(AddrMode mode) {
    static const char* ADDR_MODE_ENUM_STRING[ADDR_MODE__COUNT] = {
        "ADDR_MODE_ACCUMULATOR", "ADDR_MODE_IMMEDIATE",   "ADDR_MODE_ZERO_PAGE",
        "ADDR_MODE_ZERO_PAGE_X", "ADDR_MODE_ZERO_PAGE_Y", "ADDR_MODE_ABSOLUTE",
        "ADDR_MODE_ABSOLUTE_X",  "ADDR_MODE_ABSOLUTE_Y",  "ADDR_MODE_INDIRECT",
        "ADDR_MODE_INDIRECT_X",  "ADDR_MODE_INDIRECT_Y",  "ADDR_MODE_RELATIVE",
        "ADDR_MODE_IMPLIED",
    };
    return ADDR_MODE_ENUM_STRING[mode];
}
