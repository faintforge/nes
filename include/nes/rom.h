#ifndef ROM_H
#define ROM_H

#include "nes/common.h"

typedef enum NesRomVariation {
    NES_ROM_VARIATION_ERROR = -1,

    INES_ARCHAIC,
    INES07,
    INES,
    NES20,

    NES_ROM_VARIATION__COUNT,
} NesRomVariation;

static const char* const NES_ROM_VARIATION_STRING[NES_ROM_VARIATION__COUNT] = {
    "archac iNES", "iNES 0.7", "iNES", "NES 2.0",
};

typedef struct NesRom NesRom;
struct NesRom {
    u8* full_rom;
    NesRomVariation variation;

    const u8* prg_rom;
    const u8* chr_rom;
    u32 prg_rom_size;
    u32 chr_rom_size;

    // TODO: Figure out how to handle PRG-/CHR-(NV)RAM
    u32 prg_ram_size;
    u32 chr_ram_size;
    u32 prg_nvram_size;
    u32 chr_nvram_size;

    u8 nametable_layout;
    u8 battery;
    b8 has_trainer;
    u8 alt_nametables;
    u16 mapper;
    u8 submapper;
    u8 misc_roms;
    u8 expansion_device;

    const u8* trainer;
};

extern NesRom load_nes_rom_from_file(const char* filename);
extern void print_nes_rom(NesRom rom);

#endif // ROM_H
