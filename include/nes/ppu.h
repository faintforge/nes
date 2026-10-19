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

typedef enum VramIncrement {
    VRAM_INCREMENT_ACROSS,
    VRAM_INCREMENT_DOWN,
} VramIncrement;

typedef enum SpriteSize {
    SPRITE_SIZE_8X8,
    SPRITE_SIZE_8X16,
} SpriteSize;

typedef struct PpuMask PpuMask;
struct PpuMask {
    b8 grayscale : 1;
    b8 left_background : 1;
    b8 left_sprite : 1;
    b8 background : 1;
    b8 sprite : 1;
    b8 emph_red : 1;
    b8 emph_green : 1;
    b8 emph_blue : 1;
};

typedef struct Ppu Ppu;
struct Ppu {
    // RGB format
    u8 pixels[OUTPUT_WIDTH*OUTPUT_HEIGHT*3];

    Color palette[512];
    b8 palette_has_emph;
    u8 palette_indices[0x20];

    u16 v;
    u16 t;
    u8 x;
    u8 w;

    PpuMask mask;

    // [Reference](https://www.nesdev.org/wiki/PPU_registers)
    u16 base_nametable_address;
    VramIncrement vram_increment;
    u16 sprite_pattern_table_address;
    u16 background_pattern_address;
    SpriteSize sprite_size;
    b8 vblank_nmi;

    u8 oam_addr;
};

extern Ppu ppu_init(void);
extern void ppu_step(Ppu* ppu);

// Reads .pal formatted data. Expects 'palette' to be filled with 3-byte long
// colors in RGB order. 'palette_size' should reflect the byte-count of
// 'palette', eg. the amount of colors*3.
extern void ppu_load_palette(Ppu* ppu, const u8* palette, u32 palette_size);
extern Color ppu_decode_color(const Ppu* ppu, u8 color);

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
extern u8 ppu_status(Ppu* ppu);
extern u8 ppu_oam_data_read(const Ppu* ppu);
extern u8 ppu_data_read(const Ppu* ppu);

// extern void ppu_oam_mdma(Ppu* ppu, u8 value);

extern void ppu__debug_fill_pixels(Ppu* ppu);

#endif // PPU_H
