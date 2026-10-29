#ifndef PPU_H
#define PPU_H

#include "nes/common.h"

#define PICTURE_WIDTH 256
#define PICTURE_HEIGHT 240

#define LEFT_BORDER_WIDTH 16
#define RIGHT_BORDER_WIDTH 11
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

// https://www.nesdev.org/wiki/PPU_OAM#OAM_(Sprite)_Data
typedef struct SpriteOam SpriteOam;
struct SpriteOam {
    u8 y;
    u8 tile_index;
    u8 attrib;
    u8 x;
};

typedef struct PpuShifterCarry PpuShifterCarry;
struct PpuShifterCarry {
    u8 plane_low : 1;
    u8 plane_high : 1;
    u8 palette_low : 1;
    u8 palette_high : 1;
};

typedef struct Ppu Ppu;
struct Ppu {
    MemoryBus bus;
    u16 address_bus;
    u8 vram_bus;
    u8 io_bus;
    u8 read_data_latch;

    Color palette[512];
    b8 palette_has_emph;
    u8 palette_indices[0x20];

    u16 v;
    u16 t;
    u8 fine_x;
    u8 w;

    PpuMask mask;

    u8 sprite_eval_state;
    u8 oam_addr;
    // Sprite eval
    u8 primary_oam[64][4];
    u8 secondary_oam[8][4];
    u8 oam_data_latch;
    b8 oam_can_write;
    b8 oam_copying_sprite;
    // Index counters for sprite eval
    u8 primary_oam_index;
    u8 secondary_oam_index;
    i8 oam_copy_idx;

    SpriteOam to_render_oam[8];
    u8 to_render_count;

    // [Reference](https://www.nesdev.org/wiki/PPU_registers)
    VramIncrement vram_increment;
    u16 sprite_pattern_table_address;
    u16 background_pattern_address;
    SpriteSize sprite_size;
    b8 vblank_nmi_enabled;

    i8 ppu_address_delay;

    // TODO: Make these bit-flags
    b8 sprite_overflow_flag;
    b8 sprite0_hit_flag;
    b8 vblank_flag;
    u8 vblank_nmi_signal;

    u8 nt_bus;
    u8 at_bus;
    u8 pattern_bus_low;
    u8 pattern_bus_high;

    u16 shifter_plane_low;
    u16 shifter_plane_high;
    u16 shifter_palette_low;
    u16 shifter_palette_high;
    PpuShifterCarry last_shifted;

    i16 scanline;
    u16 dot;
    b8 is_frame_odd;
    u64 cycle;
};

typedef struct PpuDot PpuDot;
struct PpuDot {
    b8 visible;
    Color color;
    // Dot
    u16 x;
    // Scanline
    u16 y;
};

extern Ppu ppu_init(MemoryBus bus);
extern PpuDot ppu_step(Ppu* ppu);

// Reads .pal formatted data. Expects 'palette' to be filled with 3-byte long
// colors in RGB order. 'palette_size' should reflect the byte-count of
// 'palette', eg. the amount of colors*3.
extern void ppu_load_palette(Ppu* ppu, const u8* palette, u32 palette_size);
extern Color ppu_decode_color(const Ppu* ppu, u8 color);

// Memory mapped operations
extern void ppu_ctrl(Ppu* ppu, u8 value);
extern void ppu_mask(Ppu* ppu, u8 value);
extern u8   ppu_status(Ppu* ppu);
extern void ppu_oam_address(Ppu* ppu, u8 value);
extern u8   ppu_oam_data_read(Ppu* ppu);
extern void ppu_oam_data_write(Ppu* ppu, u8 value);
extern void ppu_scroll(Ppu* ppu, u8 value);
extern void ppu_address(Ppu* ppu, u8 value);
extern u8   ppu_data_read(Ppu* ppu);
extern void ppu_data_write(Ppu* ppu, u8 value);
extern void ppu_oam_dma(Ppu* ppu, u8 value);

#endif // PPU_H
