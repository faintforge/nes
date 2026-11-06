#include "nes/rom.h"

#include <string.h>

// =============================================================================
// Mappers
// =============================================================================

//
// CPU mapper space $4020-$FFFF
// PPU mapper space $0000-$3FFF
//

// https://www.nesdev.org/wiki/NROM
static MappedMemory mapper_nrom_cpu_cb(Mapper* mapper, u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // TODO: This is supposed to be open bus behavior
        // https://www.nesdev.org/wiki/Open_bus_behavior#CPU_open_bus
        static u8 dummy = 0xEA;
        return (MappedMemory) {
            &dummy,
            .writable = false,
        };
    } else if (address >= 0x8000) {
        u16 index = address - 0x8000;
        if (mapper->prg_rom_size == kib(16)) {
            index &= 0x3FFF;
        }

        return (MappedMemory) {
            .memory = &mapper->prg_rom[index],
            .writable = false,
        };

        // if (address >= 0x8000 && address < 0xC000) {
        //     return (MappedMemory) {
        //         .memory = &mapper->prg_rom[address - 0x8000],
        //         .writable = false,
        //     };
        // } else if (address >= 0xC000) {
        //     if (mapper->prg_rom_size > kib(16)) {
        //         return (MappedMemory) {
        //             .memory = &mapper->prg_rom[address - 0x8000],
        //             .writable = false,
        //         };
        //     } else {
        //         return (MappedMemory) {
        //             .memory = &mapper->prg_rom[address - 0xC000],
        //             .writable = false,
        //         };
        //     }
        // }
    }

    log_fatal("Mapper(CPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

// https://www.nesdev.org/wiki/NROM
static MappedMemory mapper_nrom_ppu_cb(Mapper* mapper, u16 address) {
    if (address < 0x2000) {
        if (mapper->chr_rom_size > 0) {
            assert(mapper->chr_rom_size == kib(8));
            return (MappedMemory) {
                .memory = &mapper->chr_rom[address],
                .writable = false,
            };
        } else {
            assert(mapper->chr_ram_size == kib(8));
            return (MappedMemory) {
                .memory = &mapper->chr_ram[address],
                .writable = true,
            };
        }
    } else if (address >= 0x2000 && address < 0x4000) {
        // https://www.nesdev.org/wiki/PPU_memory_map
        // Vert 0 => $2000 $2000 $2800 $2800
        // Hori 1 => $2000 $2400 $2000 $2400

        // NOTE: Don't ask me how I came up with this bit magic. It took me half an
        // hour at midnight to come up with this one-liner instead of like two
        // if statements.
        address &= ~(0x800 >> !mapper->nametable_layout);
        address &= 0xFFF;

        return (MappedMemory) {
            .memory = &mapper->vram[address],
            .writable = true,
        };
    }

    // NOTE: Palette mapping is handled internally in the PPU.

    log_fatal("Mapper(PPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

static Mapper mapper_build(NesRom* rom) {
    Mapper mapper = {0};

    u8* rom_data = rom->rom_data;

    mapper.nametable_layout = get_bit(rom_data[6], 0);
    mapper.has_battery = get_bit(rom_data[6], 1);
    mapper.has_trainer = get_bit(rom_data[6], 2);
    mapper.alt_nametables = get_bit(rom_data[6], 3);

    mapper.misc_roms = get_bits(rom_data[14], 0, 1);
    mapper.expansion_device = get_bits(rom_data[15], 0, 6);

    mapper.id = nibble_high(rom_data[6]) |
        (nibble_high(rom_data[7]) << 4);
    // NES 2.0 specific
    mapper.id |= (u16) nibble_low(rom_data[8]) << 8;
    mapper.submapper = nibble_high(rom_data[8]);

    // NOTE: This could be made into a hash map sort of deal
    switch (mapper.id) {
        // NROM
        case 0:
            mapper.cpu_cb = mapper_nrom_cpu_cb;
            mapper.ppu_cb = mapper_nrom_ppu_cb;
            break;
        default:
            log_fatal("Unsupported mapper: mapper %u, submapper %u", mapper.id, mapper.submapper);
            UNREACHABLE();
    }

    // PRG-ROM / CHR-ROM
    u16 prg_rom_size_msb = nibble_low(rom_data[9]);
    u16 chr_rom_size_msb = nibble_high(rom_data[9]);
    if (prg_rom_size_msb == 0xF || chr_rom_size_msb == 0xF) {
        log_warn("PRG-ROM or CHR-ROM size is using exponential-multiplier notation.");
    }
    mapper.prg_rom_size = rom_data[4] | prg_rom_size_msb << 8;
    mapper.chr_rom_size = rom_data[5] | chr_rom_size_msb << 8;

    mapper.prg_rom_size *= kib(16);
    mapper.chr_rom_size *= kib(8);

    // PRG-RAM / CHR-RAM
    u8 prg_ram_shift_count = nibble_low(rom_data[10]);
    u8 prg_nvram_shift_count = nibble_high(rom_data[10]);
    if (prg_ram_shift_count > 0) { mapper.prg_ram_size = 64 << prg_ram_shift_count; }
    if (prg_nvram_shift_count > 0) { mapper.prg_nvram_size = 64 << prg_nvram_shift_count; }

    u8 chr_ram_shift_count = nibble_low(rom_data[11]);
    u8 chr_nvram_shift_count = nibble_high(rom_data[11]);
    if (chr_ram_shift_count > 0) { mapper.chr_ram_size = 64 << chr_ram_shift_count; }
    if (chr_nvram_shift_count > 0) { mapper.chr_nvram_size = 64 << chr_nvram_shift_count; }

    if (mapper.chr_rom_size == 0 && chr_ram_shift_count == 0) {
        mapper.chr_ram_size = kib(8);
    }

    if (mapper.chr_ram_size > 0) {
        mapper.chr_ram = malloc(mapper.chr_ram_size);
    }

    // ROM pointers
    u32 idx = 16;

    if (mapper.has_trainer) {
        mapper.trainer = rom_data + idx;
        idx += 512;
    } else {
        mapper.trainer = NULL;
    }

    mapper.prg_rom = rom_data + idx;
    idx += mapper.prg_rom_size;

    mapper.chr_rom = rom_data + idx;
    idx += mapper.chr_rom_size;

    return mapper;
}


static NesRomVariation get_rom_variation(const u8* const rom, u32 length) {
    assert(rom != NULL);
    assert(length >= 16);

    // www.nesdev.org/wiki/INES#Variant_comparison

    const u8 MAGIC_BYTES[] = {'N', 'E', 'S', 0x1A};
    if (memcmp(MAGIC_BYTES, rom, sizeof(MAGIC_BYTES)) != 0) {
        return NES_ROM_VARIATION_UNKNOWN;
    }

    u8 prg_rom_size_msb = nibble_low(rom[9]);
    u8 chr_rom_size_msb = nibble_high(rom[9]);
    u32 size = (rom[4] | prg_rom_size_msb << 8) * kib(16) +
        (rom[5] | chr_rom_size_msb << 8) * kib(8);
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
    NesRom rom = {0};

    // Read ROM file
    u32 length = 0;
    if (!read_file(filename, NULL, &length)) {
        return rom;
    }
    u8* rom_data = malloc(length);
    if (!read_file(filename, rom_data, &length)) {
        free(rom_data);
        return rom;
    }

    rom.rom_data = rom_data;

    // Get variation
    rom.variation = get_rom_variation(rom_data, length);
    if (rom.variation != NES20) {
        log_warn("Detected %s ROM. Only NES 2.0 supported.", NES_ROM_VARIATION_STRING[rom.variation]);
    }

    // Flags 7
    if (get_bits(rom_data[7], 0, 1) != 0) {
        log_error("Unsupported system type.");
        free(rom_data);
        return (NesRom) {0};
    }

    u8 timing = get_bits(rom_data[12], 0, 1);
    if (timing != 0 && timing != 2) {
        log_error("CPU/PPU timing not supported!");
        free(rom_data);
        return (NesRom) {0};
    }

    rom.mapper = mapper_build(&rom);

    return rom;
}

void print_nes_rom(NesRom rom) {
    printf("ROM variation: %s\n", NES_ROM_VARIATION_STRING[rom.variation]);
    printf("PRG-ROM size: %u bytes\n", rom.mapper.prg_rom_size);
    printf("CHR-ROM size: %u bytes\n", rom.mapper.chr_rom_size);
    printf("PRG-RAM size: %u bytes\n", rom.mapper.prg_ram_size);
    printf("CHR-RAM size: %u bytes\n", rom.mapper.chr_ram_size);
    printf("PRG-NVRAM size: %u bytes\n", rom.mapper.prg_nvram_size);
    printf("CHR-NVRAM size: %u bytes\n", rom.mapper.chr_nvram_size);
    printf("Nametable layout: %s\n", rom.mapper.nametable_layout ? "Vertical" : "Horizontal");
    printf("Battery: %s\n", rom.mapper.has_battery ? "Present" : "Not present");
    printf("Trainer area: %s\n", rom.mapper.has_trainer ? "Present" : "Not present");
    printf("Alt nametables: %s\n", rom.mapper.alt_nametables ? "Yes" : "No");
    printf("Mapper: %d\n", rom.mapper.id);
    printf("Submapper: %d\n", rom.mapper.submapper);
    printf("Misc ROMs: %d\n", rom.mapper.misc_roms);
    printf("Default expansion device: $%02X\n", rom.mapper.expansion_device);
}
