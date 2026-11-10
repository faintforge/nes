#include "nes/op.h"

#include <stdio.h>
#include <string.h>

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
            {OP_BRK, ADDR_MODE_IMPLIED},
            {OP_BPL, ADDR_MODE_RELATIVE},
            {OP_JSR, ADDR_MODE_ABSOLUTE},
            {OP_BMI, ADDR_MODE_RELATIVE},
            {OP_RTI, ADDR_MODE_IMPLIED},
            {OP_BVC, ADDR_MODE_RELATIVE},
            {OP_RTS, ADDR_MODE_IMPLIED},
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
            {OP_PHP, ADDR_MODE_IMPLIED},
            {OP_CLC, ADDR_MODE_IMPLIED},
            {OP_PLP, ADDR_MODE_IMPLIED},
            {OP_SEC, ADDR_MODE_IMPLIED},
            {OP_PHA, ADDR_MODE_IMPLIED},
            {OP_CLI, ADDR_MODE_IMPLIED},
            {OP_PLA, ADDR_MODE_IMPLIED},
            {OP_SEI, ADDR_MODE_IMPLIED},
            {OP_DEY, ADDR_MODE_IMPLIED},
            {OP_TYA, ADDR_MODE_IMPLIED},
            {OP_TAY, ADDR_MODE_IMPLIED},
            {OP_CLV, ADDR_MODE_IMPLIED},
            {OP_INY, ADDR_MODE_IMPLIED},
            {OP_CLD, ADDR_MODE_IMPLIED},
            {OP_INX, ADDR_MODE_IMPLIED},
            {OP_SED, ADDR_MODE_IMPLIED},
        },
        // Column C
        {
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_BIT, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_JMP, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_JMP, ADDR_MODE_INDIRECT},
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

    // High nibble = AAAB
    const u8 matrix_row = (aaa << 1) | (bbb >> 2);
    // Low nibble = BBCC
    const u8 CC = 1;
    const u8 matrix_column = ((bbb & 3) << 2) | CC;

    if (matrix_row == 8 && matrix_column == 9) {
        return OP_UNDEFINED;
    }

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

    // Correct for all except cells A2 and between row [9-A]
    static const AddrMode mode0[8] = {
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE,
        ADDR_MODE_ACCUMULATOR, ADDR_MODE_ABSOLUTE,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE_X,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ABSOLUTE_X,
    };

    // Only correct for row [9-A]
    static const AddrMode mode1[8] = {
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE,
        ADDR_MODE_ACCUMULATOR, ADDR_MODE_ABSOLUTE,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ZERO_PAGE_Y,
        ADDR_MODE__UNDEFINED, ADDR_MODE_ABSOLUTE_X,
    };

    if (matrix_column == 6 && matrix_row >= 9 && matrix_row <= 0xB) {
        return (Op) {
            .type = col6ae_op[aaa],
            .addr_mode = mode1[bbb],
        };
    }

    if (matrix_column == 6 ||
        (matrix_row < 8 && (matrix_column == 0xA || matrix_column == 0xE))) {
        return (Op) {
            .type = col6ae_op[aaa],
            .addr_mode = mode0[bbb],
        };
    }

    // row >= 8 <=> aaa >= 4
    if (matrix_column == 0xA) {
        static const Op op[16] = {
            // Even though there are operations in the first 8 rows we don't
            // include those because we're not supposed to reach this stage
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_TXA, ADDR_MODE_IMPLIED},
            {OP_TXS, ADDR_MODE_IMPLIED},
            {OP_TAX, ADDR_MODE_IMPLIED},
            {OP_TSX, ADDR_MODE_IMPLIED},
            {OP_DEX, ADDR_MODE_IMPLIED},
            OP_UNDEFINED,
            {OP_NOP, ADDR_MODE_IMPLIED},
            OP_UNDEFINED,
        };

        return op[matrix_row];
    }

    if (matrix_column == 0xE) {
        static const Op op[16] = {
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            OP_UNDEFINED,
            {OP_STX, ADDR_MODE_ABSOLUTE},
            OP_UNDEFINED,
            {OP_LDX, ADDR_MODE_ABSOLUTE},
            {OP_LDX, ADDR_MODE_ABSOLUTE_Y},
            {OP_DEC, ADDR_MODE_ABSOLUTE},
            {OP_DEC, ADDR_MODE_ABSOLUTE_X},
            {OP_INC, ADDR_MODE_ABSOLUTE},
            {OP_INC, ADDR_MODE_ABSOLUTE_X},
        };
        return op[matrix_row];
    }

    return OP_UNDEFINED;
}

static Op decode_illegal(u8 opcode) {
    // https://www.oxyron.de/html/opcodes02.html
    switch (opcode) {
        // SLO
        case 0x07: return (Op) {OP_SLO, ADDR_MODE_ZERO_PAGE};
        case 0x17: return (Op) {OP_SLO, ADDR_MODE_ZERO_PAGE_X};
        case 0x03: return (Op) {OP_SLO, ADDR_MODE_INDIRECT_X};
        case 0x13: return (Op) {OP_SLO, ADDR_MODE_INDIRECT_Y};
        case 0x0F: return (Op) {OP_SLO, ADDR_MODE_ABSOLUTE};
        case 0x1F: return (Op) {OP_SLO, ADDR_MODE_ABSOLUTE_X};
        case 0x1B: return (Op) {OP_SLO, ADDR_MODE_ABSOLUTE_Y};
        // RLA
        case 0x27: return (Op) {OP_RLA, ADDR_MODE_ZERO_PAGE};
        case 0x37: return (Op) {OP_RLA, ADDR_MODE_ZERO_PAGE_X};
        case 0x23: return (Op) {OP_RLA, ADDR_MODE_INDIRECT_X};
        case 0x33: return (Op) {OP_RLA, ADDR_MODE_INDIRECT_Y};
        case 0x2F: return (Op) {OP_RLA, ADDR_MODE_ABSOLUTE};
        case 0x3F: return (Op) {OP_RLA, ADDR_MODE_ABSOLUTE_X};
        case 0x3B: return (Op) {OP_RLA, ADDR_MODE_ABSOLUTE_Y};
        // SRE
        case 0x47: return (Op) {OP_SRE, ADDR_MODE_ZERO_PAGE};
        case 0x57: return (Op) {OP_SRE, ADDR_MODE_ZERO_PAGE_X};
        case 0x43: return (Op) {OP_SRE, ADDR_MODE_INDIRECT_X};
        case 0x53: return (Op) {OP_SRE, ADDR_MODE_INDIRECT_Y};
        case 0x4F: return (Op) {OP_SRE, ADDR_MODE_ABSOLUTE};
        case 0x5F: return (Op) {OP_SRE, ADDR_MODE_ABSOLUTE_X};
        case 0x5B: return (Op) {OP_SRE, ADDR_MODE_ABSOLUTE_Y};
        // RRA
        case 0x67: return (Op) {OP_RRA, ADDR_MODE_ZERO_PAGE};
        case 0x77: return (Op) {OP_RRA, ADDR_MODE_ZERO_PAGE_X};
        case 0x63: return (Op) {OP_RRA, ADDR_MODE_INDIRECT_X};
        case 0x73: return (Op) {OP_RRA, ADDR_MODE_INDIRECT_Y};
        case 0x6F: return (Op) {OP_RRA, ADDR_MODE_ABSOLUTE};
        case 0x7F: return (Op) {OP_RRA, ADDR_MODE_ABSOLUTE_X};
        case 0x7B: return (Op) {OP_RRA, ADDR_MODE_ABSOLUTE_Y};
        // SAX
        case 0x87: return (Op) {OP_SAX, ADDR_MODE_ZERO_PAGE};
        case 0x97: return (Op) {OP_SAX, ADDR_MODE_ZERO_PAGE_Y};
        case 0x83: return (Op) {OP_SAX, ADDR_MODE_INDIRECT_X};
        case 0x8F: return (Op) {OP_SAX, ADDR_MODE_ABSOLUTE};
        // LAX
        case 0xAB: return (Op) {OP_LAX, ADDR_MODE_IMMEDIATE};
        case 0xA7: return (Op) {OP_LAX, ADDR_MODE_ZERO_PAGE};
        case 0xB7: return (Op) {OP_LAX, ADDR_MODE_ZERO_PAGE_Y};
        case 0xA3: return (Op) {OP_LAX, ADDR_MODE_INDIRECT_X};
        case 0xB3: return (Op) {OP_LAX, ADDR_MODE_INDIRECT_Y};
        case 0xAF: return (Op) {OP_LAX, ADDR_MODE_ABSOLUTE};
        case 0xBF: return (Op) {OP_LAX, ADDR_MODE_ABSOLUTE_Y};
        // DCP
        case 0xC7: return (Op) {OP_DCP, ADDR_MODE_ZERO_PAGE};
        case 0xD7: return (Op) {OP_DCP, ADDR_MODE_ZERO_PAGE_X};
        case 0xC3: return (Op) {OP_DCP, ADDR_MODE_INDIRECT_X};
        case 0xD3: return (Op) {OP_DCP, ADDR_MODE_INDIRECT_Y};
        case 0xCF: return (Op) {OP_DCP, ADDR_MODE_ABSOLUTE};
        case 0xDF: return (Op) {OP_DCP, ADDR_MODE_ABSOLUTE_X};
        case 0xDB: return (Op) {OP_DCP, ADDR_MODE_ABSOLUTE_Y};
        // ISC
        case 0xE7: return (Op) {OP_ISC, ADDR_MODE_ZERO_PAGE};
        case 0xF7: return (Op) {OP_ISC, ADDR_MODE_ZERO_PAGE_X};
        case 0xE3: return (Op) {OP_ISC, ADDR_MODE_INDIRECT_X};
        case 0xF3: return (Op) {OP_ISC, ADDR_MODE_INDIRECT_Y};
        case 0xEF: return (Op) {OP_ISC, ADDR_MODE_ABSOLUTE};
        case 0xFF: return (Op) {OP_ISC, ADDR_MODE_ABSOLUTE_X};
        case 0xFB: return (Op) {OP_ISC, ADDR_MODE_ABSOLUTE_Y};
        // ANC
        case 0x0B: return (Op) {OP_ANC, ADDR_MODE_IMMEDIATE};
        case 0x2B: return (Op) {OP_ANC, ADDR_MODE_IMMEDIATE};
        // ALR
        case 0x4B: return (Op) {OP_ALR, ADDR_MODE_IMMEDIATE};
        // ARR
        case 0x6B: return (Op) {OP_ARR, ADDR_MODE_IMMEDIATE};
        // XAA
        case 0x8B: return (Op) {OP_XAA, ADDR_MODE_IMMEDIATE};
        // AXS
        case 0xCB: return (Op) {OP_AXS, ADDR_MODE_IMMEDIATE};
        // SBC
        case 0xEB: return (Op) {OP_SBC, ADDR_MODE_IMMEDIATE};
        // AHX
        case 0x93: return (Op) {OP_AHX, ADDR_MODE_INDIRECT_Y};
        case 0x9F: return (Op) {OP_AHX, ADDR_MODE_ABSOLUTE_Y};
        // SHY
        case 0x9C: return (Op) {OP_SHY, ADDR_MODE_ABSOLUTE_X};
        // SHX
        case 0x9E: return (Op) {OP_SHX, ADDR_MODE_ABSOLUTE_Y};
        // TAS
        case 0x9B: return (Op) {OP_TAS, ADDR_MODE_ABSOLUTE_Y};
        // LAS
        case 0xBB: return (Op) {OP_LAS, ADDR_MODE_ABSOLUTE_Y};

        // NOP
        case 0x04: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE};
        case 0x0C: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE};
        case 0x14: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0x1A: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0x1C: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};
        case 0x34: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0x3A: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0x3C: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};
        case 0x44: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE};
        case 0x54: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0x5A: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0x5C: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};
        case 0x64: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE};
        case 0x74: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0x7A: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0x7C: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};
        case 0x80: return (Op) {OP_NOP, ADDR_MODE_IMMEDIATE};
        case 0x82: return (Op) {OP_NOP, ADDR_MODE_IMMEDIATE};
        case 0x89: return (Op) {OP_NOP, ADDR_MODE_IMMEDIATE};
        case 0xC2: return (Op) {OP_NOP, ADDR_MODE_IMMEDIATE};
        case 0xD4: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0xDA: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0xDC: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};
        case 0xE2: return (Op) {OP_NOP, ADDR_MODE_IMMEDIATE};
        case 0xEA: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0xF4: return (Op) {OP_NOP, ADDR_MODE_ZERO_PAGE_X};
        case 0xFA: return (Op) {OP_NOP, ADDR_MODE_IMPLIED};
        case 0xFC: return (Op) {OP_NOP, ADDR_MODE_ABSOLUTE_X};

        default: return OP_UNDEFINED;
    }
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
        result = decode_illegal(opcode);
    }
    return result;
}

void print_c_table_content(void) {
    for (u16 column = 0; column < 16; column++) {
        printf("    // Column %X\n", column);
        for (u16 row = 0; row < 16; row++) {
            u16 opcode = (row << 4) | column;
            Op op = decode_opcode(opcode);
            if (op.type == OP__UNDEFINED) {
                printf("    [0x%02X] = {OP__UNDEFINED, ADDR_MODE__UNDEFINED},\n", opcode);
            } else {
                char buffer[80] = {0};
                snprintf(buffer, 80, "    [0x%02X] = {%s,%-8s%s},", opcode,
                        OP_ENUM_STRING[op.type], "",
                        ADDR_MODE_ENUM_STRING[op.addr_mode]);
                printf("%-56s// %s %s\n", buffer, OP_PRETTY_STRING[op.type],
                        ADDR_MODE_PRETTY_STRING[op.addr_mode]);
            }
        }
    }
}

void print_c_string_table_content(void) {
    for (u16 column = 0; column < 16; column++) {
        printf("    // Column %X\n", column);
        for (u16 row = 0; row < 16; row++) {
            u16 opcode = (row << 4) | column;
            Op op = decode_opcode(opcode);
            if (op.type == OP__UNDEFINED) {
                printf("    [0x%02X] = \"undefined\",\n", opcode);
            } else {
                printf("    [0x%02X] = \"%s %s\",\n", opcode, OP_PRETTY_STRING[op.type],
                        ADDR_MODE_PRETTY_STRING[op.addr_mode]);
            }
        }
    }
}

void emit_c(const char* program) {
    printf("// =============================================================================\n");
    printf("// THIS FILE WAS GENERATED - DO NOT EDIT\n"); 
    printf("// GENERATED BY: %s\n", program);
    printf("//\n");
    printf("// [Reference](https://en.wikipedia.org/wiki/MOS_Technology_6502#Instruction_table)\n");
    printf("// =============================================================================\n");
    printf("\n");

    printf("#ifndef OPCODE_TABLE_H\n");
    printf("#define OPCODE_TABLE_H\n");
    printf("\n");

    printf("#include \"nes/op.h\"\n");
    printf("\n");

    printf("static const Op OPCODE_TABLE[256] = {\n");
    print_c_table_content();
    printf("};\n");
    printf("\n");

    printf("static const char* const OPCODE_STRING_TABLE[256] = {\n");
    print_c_string_table_content();
    printf("};\n");
    printf("\n");

    printf("#endif // OPCODE_TABLE_H\n");

    fflush(stdout);
}

void print_python_table_content(void) {
    for (u16 opcode = 0; opcode <= 0xff; opcode++) {
        Op op = decode_opcode(opcode);
        if (op.type == OP__UNDEFINED) {
            continue;
        }

        const char* const op_str = OP_PRETTY_STRING[op.type];
        const char* const mode_str = ADDR_MODE_PRETTY_STRING[op.addr_mode];
        printf("    0x%02X: {\"op\": \"%s\", \"mode\": \"%s\"},\n", opcode, op_str, mode_str);
    }
}

void emit_python(const char* program) {
    printf("# ==============================================================================\n");
    printf("# THIS FILE WAS GENERATED - DO NOT EDIT\n"); 
    printf("# GENERATED BY: %s\n", program);
    printf("#\n");
    printf("# [Reference](https://en.wikipedia.org/wiki/MOS_Technology_6502#Instruction_table)\n");
    printf("# ==============================================================================\n");
    printf("\n");
    printf("OPCODES = {\n");
    print_python_table_content();
    printf("}\n");
}

void print_help(const char* program) {
    fprintf(stderr, "Usage: %s [c|python]\n", program);
    fprintf(stderr, "    Defaults to C if no argument is given.\n");
}

i32 main(i32 argc, char** argv) {
    switch (argc) {
        case 1:
            emit_c(argv[0]);
            break;
        case 2:
            if (strcmp(argv[1], "c") == 0) {
                printf("here\n");
                emit_c(argv[0]);
            } else if (strcmp(argv[1], "python") == 0) {
                emit_python(argv[0]);
            } else {
                fprintf(stderr, "ERR: Invalid argument: %s\n", argv[1]);
                print_help(argv[0]);
                return 1;
            }
            break;
        default:
            fprintf(stderr, "ERR: Too many arguments.\n");
            print_help(argv[0]);
            return 1;
    }
    return 0;
}
