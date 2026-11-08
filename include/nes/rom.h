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
    SliceU8 prg_rom_bank0;
    SliceU8 prg_rom_bank1;
    SliceU8 chr_rom_bank0;
    SliceU8 chr_rom_bank1;
    u8 shifter;
    u8 shift_count;
    u8 prg_rom_bank_mode;
    u8 chr_bank_mode;
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
