#include "nes/ppu.h"

void ppu__debug_fill_pixels(Ppu* ppu) {
    for (i32 y = 0; y < OUTPUT_HEIGHT; y++) {
        for (i32 x = 0; x < OUTPUT_WIDTH; x++) {
            i32 idx = x+y*OUTPUT_WIDTH;
            ppu->pixels[idx*3 + 0] = ((float) x / OUTPUT_WIDTH)*255;
            ppu->pixels[idx*3 + 1] = ((float) y / OUTPUT_HEIGHT)*255;
            ppu->pixels[idx*3 + 2] = 64;
        }
    }
}
