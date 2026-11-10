#ifndef ROM_H
#define ROM_H

#include "nes/common.h"

typedef enum NesRomVariation {
    NES_ROM_VARIATION_UNKNOWN,
    INES_ARCHAIC,
    INES07,
    INES,
    NES20,

    NES_ROM_VARIATION__COUNT,
} NesRomVariation;

static const char* const NES_ROM_VARIATION_STRING[NES_ROM_VARIATION__COUNT] = {
    "Unknown", "archac iNES", "iNES 0.7", "iNES", "NES 2.0",
};

typedef enum NtLayout {
    NT_LAYOUT_ONE_SCREEN_A,
    NT_LAYOUT_ONE_SCREEN_B,
    NT_LAYOUT_HORIZONTAL,
    NT_LAYOUT_VERTICAL,
} NtLayout;

typedef struct Mapper Mapper;

typedef struct MapperCallbacks  MapperCallbacks ;
struct MapperCallbacks {
    i16 (*cpu_read)(Mapper* mapper, u16 address);
    void (*cpu_write)(Mapper* mapper, u16 address, u8 value);

    u8 (*ppu_read)(Mapper* mapper, u16 address);
    void (*ppu_write)(Mapper* mapper, u16 address, u8 value);

    // This is needed for the MMC1 mapper, and eventually others, which have
    // cycle specific behavior. The MMC1 mapper needs to keep track of
    // consecutive writes which is done through this function.
    // Make sure to run this after stepping the CPU.
    //
    // The reason this is a separate callback and not simply infered from the
    // read/write callbacks is because this approach gives us the ability to
    // bounds-check reads and writes. I want to merge the two, but for now this
    // is what I'm going with.
    void (*step)(Mapper* mapper, b8 read_cycle);
};

struct Mapper {
    u16 id;
    u8 submapper;

    // TODO: Bitflags
    NtLayout nametable_layout;
    u8 has_battery;
    b8 has_trainer;
    u8 alt_nametables;

    u8 misc_roms;
    u8 expansion_device;

    SliceU8 prg_rom;
    SliceU8 chr_rom;

    // TODO: Figure out how to handle PRG-/CHR-(NV)RAM
    u32 prg_ram_size;
    u8* chr_ram;
    u32 chr_ram_size;
    u32 prg_nvram_size;
    u32 chr_nvram_size;

    u8 vram[0x800];

    SliceU8 trainer;

    MapperCallbacks cb;

    // NOTE: MMC1 specific variables
    SliceU8 chr_mem;
    u8 chr_mem_is_writable;
    SliceU8 prg_rom_bank0;
    SliceU8 prg_rom_bank1;
    SliceU8 chr_bank0;
    SliceU8 chr_bank1;
    u8 shifter;
    u8 shift_count;
    u8 prg_rom_bank_mode;
    u8 chr_bank_mode;
    b8 last_cycle_was_write;
};

typedef struct NesRom NesRom;
struct NesRom {
    SliceU8 rom_data;
    NesRomVariation variation;

    Mapper mapper;
};

extern NesRom load_nes_rom_from_file(const char* filename);
extern void print_nes_rom(NesRom rom);

#endif // ROM_H
