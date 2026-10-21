#ifndef CPU_H
#define CPU_H

#include "nes/common.h"

enum {
    CPU_FLAG_CARRY                = 0x01,
    CPU_FLAG_ZERO                 = 0x02,
    CPU_FLAG_INTERRUPT_DISABLE    = 0x04,
    CPU_FLAG_DECIMAL              = 0x08,
    CPU_FLAG_BREAK                = 0x10,
    CPU_FLAG__EXPANSION           = 0x20,
    CPU_FLAG_OVERFLOW             = 0x40,
    CPU_FLAG_NEGATIVE             = 0x80,
};

extern void print_cpu_status(u8 status);

enum {
    WRITE,
    READ,
};

enum {
    NONE,
    NMI,
    RESET,
    IRQ,
    BRK,
};

typedef struct Cpu Cpu;
struct Cpu {
    MemoryBus bus;
    u8 bus_mode; // r/w
    u16 address_bus;
    u8 data_bus;

    // Registers
    u8 a;
    u8 x;
    u8 y;
    u16 pc;
    u8 s;
    u8 p;

    u8 nmi;
    u8 reset;
    u8 irq;

    u8 last_nmi;

    // Instruction register
    u8 ir;
    // Where in the instruction are we? t0 is always fetch the opcode
    u8 t;
    // Where data is stored between cycles. Eg: ADL in absolute addressing while
    // fetching ADH.
    u8 internal_data;
    b8 internal_carry;
    // What timing cycle did addressing finish. Needed for read-modify-write
    // instructions.
    u8 addr_ready_t;

    // -1 = nothing
    //  0 = off
    //  2 = on
    i8 delay_interrupt_disable;

    u8 interrupt_type;

    u64 cycle;
};

extern Cpu cpu_init(MemoryBus bus);
extern void cpu_reset(Cpu* cpu);

extern void cpu_step(Cpu* cpu);

#endif // CPU_H
