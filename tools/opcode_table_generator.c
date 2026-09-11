#include "nes/cpu.h"

#include <stdio.h>

static const Op OP_UNDEFINED = {
    .type = OP__UNDEFINED,
    .addr_mode = ADDR_MODE__UNDEFINED,
};

static const char* OP_PRETTY_STRING[OP__COUNT] = {
    "LDA", "STA", "LDX", "STX", "LDY", "STY", "TAX", "TXA", "TAY", "TYA",
    "ADC", "SBC", "INC", "DEC", "DEX", "INX", "INY", "DEY", "ASL", "LSR",
    "ROL", "ROR", "AND", "ORA", "EOR", "BIT", "CMP", "CPX", "CPY", "BCC",
    "BCS", "BEQ", "BNE", "BPL", "BMI", "BVC", "BVS", "JMP", "JSR", "RTS",
    "BRK", "RTI", "PHA", "PLA", "PHP", "PLP", "TXS", "TSX", "CLC", "SEC",
    "CLI", "SEI", "CLD", "SED", "CLV", "NOP",
};

static const char *ADDR_MODE_PRETTY_STRING[ADDR_MODE__COUNT] = {
    "A",     "#imm",  "zpg",    "zpg,X",  "abs",
    "abs,X", "abs,Y", "ind", "ind,X", "ind,Y", "rel",
};

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

static const char* ADDR_MODE_ENUM_STRING[ADDR_MODE__COUNT] = {
    "ADDR_MODE_ACCUMULATOR", "ADDR_MODE_IMMEDIATE", "ADDR_MODE_ZERO_PAGE",
    "ADDR_MODE_ZERO_PAGE_X", "ADDR_MODE_ABSOLUTE",  "ADDR_MODE_ABSOLUTE_X",
    "ADDR_MODE_ABSOLUTE_Y",  "ADDR_MODE_INDIRECT",  "ADDR_MODE_INDIRECT_X",
    "ADDR_MODE_INDIRECT_Y",  "ADDR_MODE_RELATIVE",
};

static Op decode_cc0(u8 aaa, u8 bbb) {
    // columns 0, 4, 8, C

    // High nibble = AAAB
    const u8 matrix_row = (aaa << 1) | (bbb >> 2);
    // Low nibble = BBCC
    const u8 CC = 0;
    const u8 matrix_column = ((bbb & 3) << 2) | CC;

    if (!(matrix_column == 0 ||
        matrix_column == 4 ||
        matrix_column == 8 ||
        matrix_column == 0xC)) {
        return OP_UNDEFINED;
    }

    static const Op op[4][16] = {
        // Column 0
        {
            {OP_BRK, ADDR_MODE_ACCUMULATOR},
            {OP_BPL, ADDR_MODE_RELATIVE},
            {OP_JSR, ADDR_MODE_ABSOLUTE},
            {OP_BMI, ADDR_MODE_RELATIVE},
            {OP_RTI, ADDR_MODE_ACCUMULATOR},
            {OP_BVC, ADDR_MODE_RELATIVE},
            {OP_RTS, ADDR_MODE_ACCUMULATOR},
            {OP_BVS, ADDR_MODE_RELATIVE},
            OP_UNDEFINED,
            {OP_BCC, ADDR_MODE_RELATIVE},
            {OP_LDY, ADDR_MODE_IMMEDIATE},
            {OP_BCS, ADDR_MODE_RELATIVE},
            {OP_CPY, ADDR_MODE_IMMEDIATE},
            {OP_BNE, ADDR_MODE_RELATIVE},
            {OP_CPX, ADDR_MODE_IMMEDIATE},
            {OP_BEQ, ADDR_MODE_RELATIVE},
        },
        // Column 4
        {
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_BIT, ADDR_MODE_ZERO_PAGE},
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_STY, ADDR_MODE_ZERO_PAGE},
            {OP_STY, ADDR_MODE_ZERO_PAGE_X},
            {OP_LDY, ADDR_MODE_ZERO_PAGE},
            {OP_LDY, ADDR_MODE_ZERO_PAGE_X},
            {OP_CPY, ADDR_MODE_ZERO_PAGE},
            OP_UNDEFINED,
            {OP_CPX, ADDR_MODE_ZERO_PAGE},
            OP_UNDEFINED,
        },
        // Column 8
        {
            {OP_PHP, ADDR_MODE_ACCUMULATOR},
            {OP_CLC, ADDR_MODE_ACCUMULATOR},
            {OP_PLP, ADDR_MODE_ACCUMULATOR},
            {OP_SEC, ADDR_MODE_ACCUMULATOR},
            {OP_PHA, ADDR_MODE_ACCUMULATOR},
            {OP_CLI, ADDR_MODE_ACCUMULATOR},
            {OP_PLA, ADDR_MODE_ACCUMULATOR},
            {OP_SEI, ADDR_MODE_ACCUMULATOR},
            {OP_DEY, ADDR_MODE_ACCUMULATOR},
            {OP_TYA, ADDR_MODE_ACCUMULATOR},
            {OP_TAY, ADDR_MODE_ACCUMULATOR},
            {OP_CLV, ADDR_MODE_ACCUMULATOR},
            {OP_INY, ADDR_MODE_ACCUMULATOR},
            {OP_CLD, ADDR_MODE_ACCUMULATOR},
            {OP_INX, ADDR_MODE_ACCUMULATOR},
            {OP_SED, ADDR_MODE_ACCUMULATOR},
        },
        // Column C
        {
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_BIT, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_JMP, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_JMP, ADDR_MODE_INDIRECT_X},
            OP_UNDEFINED,
            {OP_STY, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_LDY, ADDR_MODE_ABSOLUTE},
            {OP_LDY, ADDR_MODE_ABSOLUTE_X},
            {OP_CPY, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_CPX, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
        },
    };
    return op[matrix_column/4][matrix_row];
}

static Op decode_cc1(u8 aaa, u8 bbb) {
    static const OpType ops[8] = {
        OP_ORA, OP_AND, OP_EOR, OP_ADC,
        OP_STA, OP_LDA, OP_CMP, OP_SBC,
    };

    // Read left to right in periods of 3 columns for 2 rows starting at column 1.
    static const AddrMode mode[8] = {
        // Row 1
        ADDR_MODE_INDIRECT_X, ADDR_MODE_ZERO_PAGE,
        ADDR_MODE_IMMEDIATE, ADDR_MODE_ABSOLUTE,
        // Row 2
        ADDR_MODE_INDIRECT_Y, ADDR_MODE_ZERO_PAGE_X,
        ADDR_MODE_ABSOLUTE_Y, ADDR_MODE_ABSOLUTE_X,
    };

    return (Op) {
        .type = ops[aaa],
        .addr_mode = mode[bbb],
    };
}

static Op decode_cc2(u8 aaa, u8 bbb) {
    // High nibble = AAAB
    const u8 matrix_row = (aaa << 1) | (bbb >> 2);
    // Low nibble = BBCC
    const u8 CC = 2;
    const u8 matrix_column = ((bbb & 3) << 2) | CC;

    if (matrix_column == 2) {
        if (matrix_row == 0xA) {
            return (Op) {
                .type = OP_LDX,
                .addr_mode = ADDR_MODE_IMMEDIATE,
            };
        } else {
            return OP_UNDEFINED;
        }
    }

    // Column 6 (aaa < 8)
    // Column A and E (aaa < 4)
    static const OpType col6ae_op[8] = {
        OP_ASL, OP_ROL, OP_LSR, OP_ROR,
        OP_STX, OP_LDX, OP_DEC, OP_INC,
    };

    // Correct for all except cells A2
    static const AddrMode mode[8] = {
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE,
        ADDR_MODE_ACCUMULATOR, ADDR_MODE_ABSOLUTE,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE_X,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ABSOLUTE_X,
    };

    if (matrix_column == 6 ||
        (matrix_row < 8 && (matrix_column == 0xA || matrix_column == 0xE))) {
        return (Op) {
            .type = col6ae_op[aaa],
            .addr_mode = mode[bbb],
        };
    }

    // row >= 8 <=> aaa >= 4
    if (matrix_column == 0xA) {
        static const OpType row8f_op[16] = {
            // Even though there are operations in the first 8 rows we don't
            // include those because we're not supposed to reach this stage
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP_TXA,
            OP_TXS,
            OP_TAX,
            OP_TSX,
            OP_DEX,
            OP__UNDEFINED,
            OP_NOP,
            OP__UNDEFINED,
        };

        return (Op) {
            .type = row8f_op[matrix_row],
            .addr_mode = mode[bbb],
        };
    }

    if (matrix_column == 0xE) {
        static const OpType row8f_op[16] = {
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP__UNDEFINED,
            OP_STX,
            OP__UNDEFINED,
            OP_LDX,
            OP_LDX,
            OP_DEC,
            OP_DEC,
            OP_INC,
            OP_INC,
        };

        static const AddrMode row8f_mode[16] = {
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE_ABSOLUTE,
            ADDR_MODE__UNDEFINED,
            ADDR_MODE_ABSOLUTE,
            ADDR_MODE_ABSOLUTE_Y,
            ADDR_MODE_ABSOLUTE,
            ADDR_MODE_ABSOLUTE_X,
            ADDR_MODE_ABSOLUTE,
            ADDR_MODE_ABSOLUTE_X,
        };

        return (Op) {
            .type = row8f_op[matrix_row],
            .addr_mode = row8f_mode[matrix_row],
        };
    }

    return OP_UNDEFINED;
}

Op decode_opcode(u8 opcode) {

    // opcode structure:
    // AAABBBCC
    u8 aaa = (opcode >> 5) & 0x7;
    u8 bbb = (opcode >> 2) & 0x7;
    u8 cc = opcode & 0x3;

    // AAA describes the operation type (I think). Because the top bit of BBB is
    // included in the high nibble with AAA the instructions change every other
    // high nibble in the instruction matrix.

    // CC -> New instructions every other row repeating in periods of 3 columns.
    // (with a lot of exceptions).

    Op result = OP_UNDEFINED;
    switch (cc) {
        case 0:
            result = decode_cc0(aaa, bbb);
            break;
        case 1:
            result = decode_cc1(aaa, bbb);
            break;
        case 2:
            result = decode_cc2(aaa, bbb);
            break;
    }
    if (result.type == OP__UNDEFINED || result.addr_mode == ADDR_MODE__UNDEFINED) {
        result = OP_UNDEFINED;
    }
    return result;
}

void print_table_content(void) {
    for (u8 i = 0; i < 255; i++) {
        Op op = decode_opcode(i);
        if (op.type == OP__UNDEFINED) {
            continue;
        }
        printf("    [0x%02X] = {%s, %s}\t// %s %s,\n",
                i,
                OP_ENUM_STRING[op.type],
                ADDR_MODE_ENUM_STRING[op.addr_mode],
                OP_PRETTY_STRING[op.type],
                ADDR_MODE_PRETTY_STRING[op.addr_mode]);
    }
}

i32 main(i32 argc, char** argv) {
    (void) argc;

    printf("// =============================================================================\n");
    printf("// THIS FILE WAS GENERATED - DON'T EDIT IT\n"); 
    printf("// GENERATED BY: %s\n", argv[0]);
    printf("// =============================================================================\n");
    printf("\n");
    printf("#ifndef OPCODE_TABLE_H\n");
    printf("#define OPCODE_TABLE_H\n");
    printf("\n");
    printf("#include \"nes/cpu.h\"\n");
    printf("\n");
    printf("const Op OPCODE_TABLE[0xFF] = {\n");
    print_table_content();
    printf("};\n");
    printf("\n");
    printf("#endif // OPCODE_TABLE_H\n");

    fflush(stdout);
    return 0;
}
