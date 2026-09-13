#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/op.h"
#include "nes/opcode_table.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// FORMAT
//
// opcode: u8
// operand: u16
//
// CPU initial
// a: u8
// x: u8
// y: u8
// s: u8
// p: u8
// pc: u16
// memory:
// length: u8
// entries*length:
//      address: u16
//      value: u8
//
// CPU expected
// a: u8
// x: u8
// y: u8
// s: u8
// p: u8
// pc: u16
// memory:
// length: u8
// entries*length:
//      address: u16
//      value: u8
//
// Memory ops
// length: u8
// entires:
//     type: u8
//     address: u16
//     value: u8

#define MAX_MEMORY_ENTREIS 8
#define MAX_MEMORY_OPS 8

typedef enum MemoryOpType {
    MEM_OP_READ,
    MEM_OP_WRITE,
} MemoryOpType;

typedef struct MemoryOp MemoryOp;
struct MemoryOp {
    MemoryOpType type;
    u16 address;
    u8 value;
};

typedef struct MemoryEntry MemoryEntry;
struct MemoryEntry {
    u16 address;
    u8 value;
};

typedef struct TestCase TestCase;
struct TestCase {
    u8 opcode;
    u16 operand;

    CPU initial_cpu;
    u8 initial_memory_length;
    MemoryEntry initial_memory[MAX_MEMORY_ENTREIS];

    CPU expected_cpu;
    u8 expected_memory_length;
    MemoryEntry expected_memory[MAX_MEMORY_ENTREIS];

    u8 memory_ops_length;
    MemoryOp memory_ops[MAX_MEMORY_OPS];
};

typedef struct TestingMachine TestingMachine;
struct TestingMachine {
    CPU cpu;
    u8 memory[0x10000];
    b8 running;
    u8 result;
    MemoryOp mem_ops[MAX_MEMORY_OPS];
    u8 mem_op_i;
};

u8 bus_read(MemoryBus* bus, u16 address) {
    TestingMachine* machine = bus->ctx;
    u8 value = machine->memory[address];
    machine->mem_ops[machine->mem_op_i] = (MemoryOp) {
        .type = MEM_OP_READ,
        .address = address,
        .value = value,
    };
    machine->mem_op_i = (machine->mem_op_i+1) & (MAX_MEMORY_OPS-1);
    return value;
}

void dump_cpu(const CPU* cpu) {
    const char status_flags[16] = {
        'c', 'C',
        'z', 'Z',
        'i', 'I',
        'd', 'D',
        'b', 'B',
        ' ', ' ',
        'o', 'O',
        'n', 'N',
    };

    printf("\n===== CPU DUMP =====\n");
    printf("A      = %-5u (0x%02X)\n", cpu->a, cpu->a);
    printf("X      = %-5u (0x%02X)\n", cpu->x, cpu->x);
    printf("Y      = %-5u (0x%02X)\n", cpu->y, cpu->y);
    printf("PC     = %-5u (0x%04X)\n", cpu->pc, cpu->pc);
    printf("S      = %-5u (0x%02X)\n", cpu->s, cpu->s);
    printf("P      = ");
    for (u8 i = 0; i < 8; i++) {
        u8 value = (cpu->p >> i) & 1;
        putc(status_flags[i*2 + value], stdout);
    }
    printf("\n");
    printf("cycle  = %lu\n", cpu->cycle);
    printf("====================\n\n");
}

void bus_write(MemoryBus* bus, u16 address, u8 value) {
    TestingMachine* machine = bus->ctx;
    if (address >= 0x4000 && address <= 0x7FFF) {
        switch (address) {
            // Reset machine state but keep executing
            case 0x4000:
                memset(machine->memory, 0, 0x8000);
                CPU* cpu = &machine->cpu;
                cpu->p = CPU_STATUS_INTERRUPT_DISABLE;
                cpu->a = 0;
                cpu->x = 0;
                cpu->y = 0;
                cpu->s = 0xFD;
                break;
            case 0x4001:
                dump_cpu(&machine->cpu);
                break;
            case 0x7FFF:
                machine->result = value;
                machine->running = false;
                break;
        }
    }

    if (address >= 0x8000) {
        fprintf(stderr, "ERR: Attempted to write to ROM [0x8000-0xFFFF]");
        exit(1);
    }

    machine->mem_ops[machine->mem_op_i] = (MemoryOp) {
        .type = MEM_OP_WRITE,
        .address = address,
        .value = value,
    };
    machine->mem_op_i = (machine->mem_op_i+1) & (MAX_MEMORY_OPS-1);

    machine->memory[address] = value;
}

b8 load_rom(TestingMachine* machine, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERR: Failed to read %s: %s\n", filename, strerror(errno));
        return false;
    }
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(machine->memory+0x8000, len, 1, fp);
    fclose(fp);
    return true;
}

static inline u8 read_u8(FILE* fp) {
    i32 result = fgetc(fp);
    if (result == EOF) {
        exit(1);
    }
    return result;
}

static inline u16 read_u16(FILE* fp) {
    i32 low = fgetc(fp);
    if (low == EOF) {
        exit(1);
    }
    i32 high = fgetc(fp);
    if (high == EOF) {
        exit(1);
    }
    return (high << 8) | low;
}

CPU read_cpu(FILE* fp) {
    CPU cpu = {0};
    cpu.a = read_u8(fp);
    cpu.x = read_u8(fp);
    cpu.y = read_u8(fp);
    cpu.s = read_u8(fp);
    cpu.p = read_u8(fp);
    cpu.pc = read_u16(fp);
    return cpu;
}

void read_memory(u8* length, MemoryEntry* entries, FILE* fp) {
    *length = read_u8(fp);
    assert(*length <= MAX_MEMORY_ENTREIS);
    for (u8 i = 0; i < *length; i++) {
        entries[i].address = read_u16(fp);
        entries[i].value = read_u8(fp);
    }
}

void read_memory_ops(TestCase* test, FILE* fp) {
    u8 length = read_u8(fp);
    assert(length <= MAX_MEMORY_ENTREIS);
    test->memory_ops_length = length;
    for (u8 i = 0; i < length; i++) {
        test->memory_ops[i].type = read_u8(fp);
        test->memory_ops[i].address = read_u16(fp);
        test->memory_ops[i].value = read_u8(fp);
    }
}

TestCase load_test_file(const char* filename) {
    TestCase test = {0};

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERR: Failed to read %s: %s\n", filename, strerror(errno));
        return test;
    }

    test.opcode = read_u8(fp);
    test.operand = read_u16(fp);

    test.initial_cpu = read_cpu(fp);
    read_memory(&test.initial_memory_length, test.initial_memory, fp);

    test.expected_cpu = read_cpu(fp);
    read_memory(&test.expected_memory_length, test.expected_memory, fp);

    read_memory_ops(&test, fp);

    assert(fgetc(fp) == EOF);
    fclose(fp);

    return test;
}

void pretty_print_opcode(u8 opcode) {
    Op op = OPCODE_TABLE[opcode];
    printf("%02X: %s %s", opcode, op_pretty_string(op.type), addr_mode_pretty_string(op.addr_mode));
}

i32 main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [ROM_FILE]\n", argv[0]);
        return 1;
    }

    TestCase test = load_test_file(argv[1]);

    // Set up CPU
    TestingMachine machine = {
        .running = true,
    };
    MemoryBus bus = {
        .ctx = &machine,
        .read = bus_read,
        .write = bus_write,
    };
    CPU cpu = test.initial_cpu;
    cpu.bus = bus;

    // Set up ROM
    machine.memory[0x8000] = test.opcode;
    if (test.operand <= 0xFF) {
        machine.memory[0x8001] = test.operand;
    } else {
        machine.memory[0x8001] = test.operand & 0xFF;
        machine.memory[0x8002] = test.operand >> 8;
    }

    // Set up memory
    for (u8 i = 0; i < test.initial_memory_length; i++) {
        MemoryEntry op = test.initial_memory[i];
        machine.memory[op.address] = op.value;
    }

    cpu_step(&cpu);

    b8 passed = true;

    // Check registers
    CPU expected = test.expected_cpu;
    if (!(cpu.a == expected.a &&
        cpu.x == expected.x &&
        cpu.y == expected.y &&
        cpu.pc == expected.pc &&
        cpu.s == expected.s &&
        cpu.p == expected.p)) {
        passed = false;
    }

    // Check relevant memory
    for (u8 i = 0; i < test.expected_memory_length; i++) {
        MemoryEntry entry = test.expected_memory[i];
        if (machine.memory[entry.address] != entry.value) {
            passed = false;
            printf("ERR: Memory\n");
            break;
        }
    }

    // Check memory operations
    if (test.memory_ops_length != machine.mem_op_i) {
        passed = false;
        printf("ERR: Memory op\n");
        printf("%d, %d\n", test.memory_ops_length, machine.mem_op_i);
    }

    for (u8 i = 0; i < test.memory_ops_length; i++) {
        MemoryOp expected_op = test.memory_ops[i];
        MemoryOp actual_op = machine.mem_ops[i];
        if (memcmp(&expected_op, &actual_op, sizeof(MemoryOp)) != 0) {
            passed = false;
            printf("ERR: Memory op\n");
            printf("Expected: %s, $%04X, %d ($%d)\n",
                    expected_op.type ? "write" : "read",
                    expected_op.address,
                    expected_op.value,
                    expected_op.value);
            printf("Performed: %s, $%04X, %d ($%d)\n",
                    actual_op.type ? "write" : "read",
                    actual_op.address,
                    actual_op.value,
                    actual_op.value);
            break;
        }
    }

    printf("%d\n", passed);

    return 0;

    // TestingMachine machine = {
    //     .running = true,
    // };
    // MemoryBus bus = {
    //     .ctx = &machine,
    //     .read = bus_read,
    //     .write = bus_write,
    // };
    // machine.cpu = cpu_init(bus);
    // if (!load_rom(&machine, argv[1])) {
    //     return 1;
    // }
    //
    // cpu_reset(&machine.cpu);
    // while (machine.running) {
    //     u16 addr = machine.cpu.pc;
    //     u8 opcode = bus_read(&bus, addr);
    //
    //     u8 cycles = cpu_step(&machine.cpu);
    //
    //     printf("%04X:    ", addr);
    //     pretty_print_opcode(opcode);
    //     printf("\tcycles: %u\n", cycles);
    //
    //     if ((machine.cpu.p & CPU_STATUS_BREAK) != 0) {
    //         fprintf(stderr, "ERR: Break flag set in CPU.");
    //     }
    // }

    return machine.result;
}
