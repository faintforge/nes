#ifndef DMA_H
#define DMA_H

#include "nes/common.h"
#include "nes/cpu.h"
#include "nes/ppu.h"

typedef enum DmaState {
    DMA_IDLE,
    DMA_PENDING,
    DMA_GET,
    DMA_PUT,
} DmaState;

typedef struct OamDma OamDma;
struct OamDma {
    DmaState state;
    u8 page;
    u16 index;
    u8 data_latch;
};

extern void oam_dma_signal(OamDma* dma, u8 page);
extern void oam_dma_step(OamDma* dma, Cpu* cpu, Ppu* ppu);

#endif // DMA_H
