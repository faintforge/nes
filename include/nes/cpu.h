#ifndef CPU_H
#define CPU_H

#include "nes/common.h"

enum {
    CPU_STATUS_CARRY                = 0x01,
    CPU_STATUS_ZERO                 = 0x02,
    CPU_STATUS_INTERRUPT_DISABLE    = 0x04,
    CPU_STATUS_DECIMAL              = 0x08,
    CPU_STATUS_BREAK                = 0x10,
    CPU_STATUS__EXPANSION           = 0x20,
    CPU_STATUS_OVERFLOW             = 0x40,
    CPU_STATUS_NEGATIVE             = 0x80,
};

extern void print_cpu_status(u8 status);

typedef struct CPU CPU;
struct CPU {
    u8* memory;

    u8 a;
    u8 x;
    u8 y;
    u16 pc;
    u8 s;
    u8 p;

    u64 cycle;
};

extern CPU cpu_create(void);
extern void cpu_destroy(CPU* cpu);
extern void cpu_reset(CPU* cpu);

extern u8 cpu_step(CPU* cpu);

extern void cpu_irq(CPU* cpu);
extern void cpu_nmi(CPU* cpu);

#endif // CPU_H
