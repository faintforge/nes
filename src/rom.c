#include "nes/rom.h"

#include <string.h>

static NesRomVariation get_rom_variation(const u8* const rom, u32 length) {
    assert(rom != NULL);
    assert(length >= 16);

    // www.nesdev.org/wiki/INES#Variant_comparison

    const u8 MAGIC_BYTES[] = {'N', 'E', 'S', 0x1A};
    if (memcmp(MAGIC_BYTES, rom, sizeof(MAGIC_BYTES)) != 0) {
        return NES_ROM_VARIATION_ERROR;
    }

    u8 prg_rom_size_msb = nibble_low(rom[9]);
    u8 chr_rom_size_msb = nibble_high(rom[9]);
    u32 size = (rom[4] | prg_rom_size_msb << 4) * kib(16) +
        (rom[5] | chr_rom_size_msb << 4) * kib(8);
    if ((rom[7] & 0x0C) == 0x08 && size <= length) {
        return NES20;
    }

    if ((rom[7] & 0x0C) == 0x04) {
        return INES_ARCHAIC;
    }

    if ((rom[7] & 0x0C) == 0x00 &&
            rom[12] == 0x00 &&
            rom[13] == 0x00 &&
            rom[14] == 0x00 &&
            rom[15] == 0x00) {
        return INES;
    }

    return INES07;
}

NesRom load_nes_rom_from_file(const char* filename) {
    NesRom nrom = {0};

    // Read ROM file
    u32 length = 0;
    if (!read_file(filename, NULL, &length)) {
        return nrom;
    }
    u8* rom = malloc(length);
    if (!read_file(filename, rom, &length)) {
        free(rom);
        return nrom;
    }

    nrom.full_rom = rom;

    // Get variation
    nrom.variation = get_rom_variation(rom, length);
    if (nrom.variation != NES20) {
        fprintf(stderr, "WARN: Detected %s ROM. Only NES 2.0 supported.\n", NES_ROM_VARIATION_STRING[nrom.variation]);
    }

    // Parse header
    nrom.prg_rom_size = rom[4];
    nrom.chr_rom_size = rom[5];

    // Flags 6
    nrom.nametable_layout = get_bit(rom[6], 0);
    nrom.battery = get_bit(rom[6], 1);
    nrom.has_trainer = get_bit(rom[6], 2);
    nrom.alt_nametables = get_bit(rom[6], 3);
    nrom.mapper = nibble_high(rom[6]);

    // Flags 7
    if (get_bits(rom[7], 0, 1) != 0) {
        fprintf(stderr, "ERR: Unsupported system type.\n");
        free(rom);
        return (NesRom) {0};
    }
    nrom.mapper |= nibble_high(rom[9]) << 4;

    nrom.mapper |= (u16) nibble_low(rom[8]) << 8;
    nrom.submapper = nibble_high(rom[8]);

    u8 prg_rom_size_msb = nibble_low(rom[9]);
    u8 chr_rom_size_msb = nibble_high(rom[9]);
    if (prg_rom_size_msb == 0xF || chr_rom_size_msb) {
        fprintf(stderr, "WARN: PRG-ROM or CHR-ROM size is using exponential-multiplier notation.\n");
    }
    nrom.prg_rom_size |= (u16) prg_rom_size_msb;
    nrom.chr_rom_size |= (u16) chr_rom_size_msb;

    nrom.prg_rom_size *= kib(16);
    nrom.chr_rom_size *= kib(8);

    nrom.prg_ram_size = 64 << nibble_low(rom[10]);
    nrom.prg_nvram_size = 64 << nibble_high(rom[10]);
    if (nrom.prg_ram_size == 64) { nrom.prg_ram_size = 0; }
    if (nrom.prg_nvram_size == 64) { nrom.prg_nvram_size = 0; }

    nrom.chr_ram_size = 64 << nibble_low(rom[11]);
    nrom.chr_nvram_size = 64 << nibble_high(rom[11]);
    if (nrom.chr_ram_size == 64) { nrom.chr_ram_size = 0; }
    if (nrom.chr_nvram_size == 64) { nrom.chr_nvram_size = 0; }

    u8 timing = get_bits(rom[12], 0, 1);
    if (timing != 0 && timing != 2) {
        fprintf(stderr, "ERR: CPU/PPU timing not supported!\n");
        free(rom);
        return (NesRom) {0};
    }

    nrom.misc_roms = get_bits(rom[14], 0, 1);
    nrom.expansion_device = get_bits(rom[15], 0, 6);

    u32 idx = 16;

    if (nrom.has_trainer) {
        nrom.trainer = rom + idx;
        idx += 512;
    } else {
        nrom.trainer = NULL;
    }

    nrom.prg_rom = rom + idx;
    idx += nrom.prg_rom_size;

    nrom.chr_rom = rom + idx;
    idx += nrom.chr_rom_size;

    return nrom;
}

void print_nes_rom(NesRom rom) {
    printf("ROM variation: %s\n", NES_ROM_VARIATION_STRING[rom.variation]);
    printf("PRG-ROM size: %u bytes\n", rom.prg_rom_size);
    printf("CHR-ROM size: %u bytes\n", rom.chr_rom_size);
    printf("PRG-RAM size: %u bytes\n", rom.prg_ram_size);
    printf("CHR-RAM size: %u bytes\n", rom.chr_ram_size);
    printf("PRG-NVRAM size: %u bytes\n", rom.prg_nvram_size);
    printf("CHR-NVRAM size: %u bytes\n", rom.chr_nvram_size);
    printf("Nametable layout: %d\n", rom.nametable_layout);
    printf("Battery: %s\n", rom.battery ? "Present" : "Not present");
    printf("Trainer area: %s\n", rom.has_trainer ? "Present" : "Not present");
    printf("Alt nametables: %s\n", rom.alt_nametables ? "Yes" : "No");
    printf("Mapper: %d\n", rom.mapper);
    printf("Submapper: %d\n", rom.submapper);
    printf("Misc ROMs: %d\n", rom.misc_roms);
    printf("Default expansion device: $%02X\n", rom.expansion_device);
}
