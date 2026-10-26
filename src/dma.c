#include "nes/dma.h"

void oam_dma_signal(OamDma* dma, u8 page) {
    *dma = (OamDma) {
        .state = DMA_PENDING,
        .page = page,
        .index = 0,
        .data_latch = 0,
    };
}

void oam_dma_step(OamDma* dma, Cpu* cpu, Ppu* ppu) {
    switch (dma->state) {
        case DMA_IDLE:
            break;
        case DMA_PENDING:
            cpu->rdy = 1;
            if (cpu_is_halted(cpu)) {
                dma->state = DMA_GET;
            }
            break;
        case DMA_GET:
            dma->state = DMA_PUT;
            u16 address = (dma->page << 8) | dma->index;
            dma->data_latch = cpu->bus.read(&cpu->bus, address);
            dma->index++;
            break;
        case DMA_PUT:
            dma->state = DMA_GET;
            ppu_oam_data_write(ppu, dma->data_latch);
            if (dma->index == 256) {
                dma->state = DMA_IDLE;
                cpu->rdy = 0;
            }
            break;
    }
}
