#ifndef CPU_H
#define CPU_H

#include "nes/common.h"

enum {
    FLAG_CARRY                = 0x01,
    FLAG_ZERO                 = 0x02,
    FLAG_INTERRUPT_DISABLE    = 0x04,
    FLAG_DECIMAL              = 0x08,
    FLAG_BREAK                = 0x10,
    FLAG__EXPANSION           = 0x20,
    FLAG_OVERFLOW             = 0x40,
    FLAG_NEGATIVE             = 0x80,
};

extern void print_cpu_status(u8 status);

typedef struct MemoryBus MemoryBus;
struct MemoryBus {
    u8 (*read)(MemoryBus* bus, u16 address);
    void (*write)(MemoryBus* bus, u16 address, u8 value);
    void* ctx;
};

typedef struct CPU CPU;
struct CPU {
    MemoryBus bus;

    u8 a;
    u8 x;
    u8 y;
    u16 pc;
    u8 s;
    u8 p;

    u64 cycle;
};

extern CPU cpu_init(MemoryBus bus);
extern void cpu_reset(CPU* cpu);

extern u8 cpu_step(CPU* cpu);

extern void cpu_irq(CPU* cpu);
extern void cpu_nmi(CPU* cpu);

#endif // CPU_H
