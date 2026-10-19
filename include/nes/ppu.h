#ifndef PPU_H
#define PPU_H

#include "nes/common.h"

#define PICTURE_WIDTH 256
#define PICTURE_HEIGHT 240

#define LEFT_BORDER_WIDTH 16
#define RIGHT_BORDER_WIDTH 16
#define BOTTOM_BORRDER_HEIGHT 2

#define OUTPUT_WIDTH (PICTURE_WIDTH+LEFT_BORDER_WIDTH+RIGHT_BORDER_WIDTH)
#define OUTPUT_HEIGHT (PICTURE_HEIGHT+BOTTOM_BORRDER_HEIGHT)

typedef struct Ppu Ppu;
struct Ppu {
    // RGB format
    u8 pixels[OUTPUT_WIDTH*OUTPUT_HEIGHT*3];
};

// Write
extern void ppu_crtl(Ppu* ppu, u8 value);
extern void ppu_mask(Ppu* ppu, u8 value);
extern void ppu_oam_address(Ppu* ppu, u8 value);
extern void ppu_oam_data_write(Ppu* ppu, u8 value);
extern void ppu_data_write(Ppu* ppu, u8 value);

// Write 2x
extern void ppu_scroll(Ppu* ppu, u8 value);
extern void ppu_address(Ppu* ppu, u8 value);

// Read
extern u8   ppu_status(const Ppu* ppu);
extern u8   ppu_oam_data_read(const Ppu* ppu);
extern u8   ppu_data_read(const Ppu* ppu);

extern void ppu__debug_fill_pixels(Ppu* ppu);

// extern void ppu_oam_mdma(Ppu* ppu, u8 value);

#endif // PPU_H
