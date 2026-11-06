#include "nes/rom.h"

#include <string.h>

// =============================================================================
// Mappers
// =============================================================================

//
// CPU mapper space $4020-$FFFF
// PPU mapper space $0000-$3FFF
//

typedef struct MappedMemory MappedMemory;
struct MappedMemory {
    u8* memory;
    b8 writable;
};

static MappedMemory mapper_util_vram(Mapper* mapper, u16 address) {
    assert(address >= 0x2000 && address < 0x4000);

    switch (mapper->nametable_layout) {
        case NT_LAYOUT_ONE_SCREEN_A:
            address &= 0x3FF;
            break;
        case NT_LAYOUT_ONE_SCREEN_B:
            address &= 0x3FF;
            address += 0x400;
            break;
        case NT_LAYOUT_VERTICAL:
            // Turn off vertical nametable bit
            address &= ~0x400;
            address &= 0xFFF;
            break;
        case NT_LAYOUT_HORIZONTAL:
            // Turn off horizontal nametable bit
            address &= ~0x800;
            address &= 0xFFF;
            break;
    }

    return (MappedMemory) {
        .memory = &mapper->vram[address],
        .writable = true,
    };
}

// https://www.nesdev.org/wiki/NROM
static MappedMemory mapper_nrom_cpu_helper(Mapper* mapper, u16 address) {
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
    }

    log_fatal("Mapper(CPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

static u8 mapper_nrom_cpu_read(Mapper* mapper, u16 address) {
    return *mapper_nrom_cpu_helper(mapper, address).memory;
}

static void mapper_nrom_cpu_write(Mapper* mapper, u16 address, u8 value) {
    MappedMemory mapped = mapper_nrom_cpu_helper(mapper, address);
    if (mapped.writable) {
        *mapped.memory = value;
    }
}

// https://www.nesdev.org/wiki/NROM
static MappedMemory mapper_nrom_ppu_helper(Mapper* mapper, u16 address) {
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
        return mapper_util_vram(mapper, address);
    }

    // NOTE: Palette mapping is handled internally in the PPU.

    log_fatal("Mapper(PPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

static u8 mapper_nrom_ppu_read(Mapper* mapper, u16 address) {
    return *mapper_nrom_ppu_helper(mapper, address).memory;
}

static void mapper_nrom_ppu_write(Mapper* mapper, u16 address, u8 value) {
    MappedMemory mapped = mapper_nrom_ppu_helper(mapper, address);
    if (mapped.writable) {
        *mapped.memory = value;
    }
}

// https://www.nesdev.org/wiki/MMC1
static u8 mapper_mmc1_cpu_read(Mapper* mapper, u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // TODO: Optional PRG-RAM
        // UNREACHABLE();
        return 0xEA;
    } else if (address >= 0x8000 && address < 0xC000) {
        return mapper->prg_rom_bank0[address - 0x8000];
    } else if (address >= 0xC000) {
        return mapper->prg_rom_bank1[address - 0xC000];
    }

    log_fatal("Mapper(CPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

static void mapper_mmc1_control(Mapper* mapper) {
    // https://www.nesdev.org/wiki/MMC1#Control_(internal,_$8000-$9FFF)
    mapper->nametable_layout = get_bits(mapper->shifter, 0, 1);
    mapper->prg_rom_bank_mode = get_bits(mapper->shifter, 2, 3);
    mapper->chr_bank_mode = get_bit(mapper->shifter, 4);

    if (mapper->submapper == 5) {
        assert(mapper->prg_rom_bank_mode == 0);
    }
}

static void mapper_mmc1_chr_bank0(Mapper* mapper) {
    // Switch 8 KiB
    if (mapper->chr_bank_mode == 0) {
        u32 offset = (mapper->shifter & ~0x1)*kib(8);
        mapper->chr_rom_bank0 = mapper->chr_rom + offset;
        mapper->chr_rom_bank1 = mapper->chr_rom + offset + kib(4);
    }
    // Switch 4 KiB
    else {
        u32 offset = mapper->shifter*kib(4);
        mapper->chr_rom_bank0 = mapper->chr_rom + offset;
    }
}

static void mapper_mmc1_chr_bank1(Mapper* mapper) {
    if (mapper->chr_bank_mode == 0) {
        return;
    }

    u32 offset = mapper->shifter*kib(4);
    mapper->chr_rom_bank1 = mapper->chr_rom + offset;
}

static void mapper_mmc1_prg_bank(Mapper* mapper) {
    u32 offset = -1;
    switch (mapper->prg_rom_bank_mode) {
        // Switchable 32 KiB
        case 0:
        case 1:
            offset = (mapper->shifter & ~0x1)*kib(32);
            mapper->prg_rom_bank0 = mapper->prg_rom + offset;
            mapper->prg_rom_bank1 = mapper->prg_rom + offset + kib(16);
            break;
        // Fixed 0, switchable 1
        case 2:
            offset = mapper->shifter*kib(16);
            mapper->prg_rom_bank0 = mapper->prg_rom;
            mapper->prg_rom_bank1 = mapper->prg_rom + offset;
            break;
        // Switchable 0, fixed 1
        case 3:
            offset = mapper->shifter*kib(16);
            mapper->prg_rom_bank0 = mapper->prg_rom + offset;
            mapper->prg_rom_bank1 = mapper->prg_rom + mapper->prg_rom_size - kib(16);
            break;
    }
}

static void mapper_mmc1_cpu_write(Mapper* mapper, u16 address, u8 value) {
    // https://www.nesdev.org/wiki/MMC1#Reset
    if (get_bit(value, 7) == 1) {
        mapper->shifter = 0;
    } else {
        mapper->shifter |= get_bit(value, 0) << mapper->shift_count;
        mapper->shift_count++;

        if (mapper->shift_count == 5) {
            u8 reg = get_bits(address, 13, 14);
            switch (reg) {
                case 0:
                    mapper_mmc1_control(mapper);
                    break;
                case 1:
                    mapper_mmc1_chr_bank0(mapper);
                    break;
                case 2:
                    mapper_mmc1_chr_bank1(mapper);
                    break;
                case 3:
                    mapper_mmc1_prg_bank(mapper);
                    break;
                default:
                    UNREACHABLE();
            }

            mapper->shifter = 0;
            mapper->shift_count = 0;
        }
    }
}

static MappedMemory mapper_mmc1_ppu_helper(Mapper* mapper, u16 address) {
    if (address < 0x1000) {
        return (MappedMemory) {
            .memory = &mapper->chr_rom_bank0[address],
            .writable = false,
        };
    } else if (address >= 0x1000 && address < 0x2000) {
        return (MappedMemory) {
            .memory = &mapper->chr_rom_bank1[address - 0x1000],
            .writable = false,
        };
    } else if (address >= 0x2000 && address < 0x4000) {
        return mapper_util_vram(mapper, address);
    }

    log_fatal("Mapper(PPU): Out-of-bounds: "HEX_U16, address);
    UNREACHABLE();
}

static u8 mapper_mmc1_ppu_read(Mapper* mapper, u16 address) {
    return *mapper_mmc1_ppu_helper(mapper, address).memory;
}

static void mapper_mmc1_ppu_write(Mapper* mapper, u16 address, u8 value) {
    MappedMemory mapped = mapper_mmc1_ppu_helper(mapper, address);
    if (mapped.writable) {
        *mapped.memory = value;
    }
}

static Mapper mapper_build(NesRom* rom) {
    Mapper mapper = {0};

    u8* rom_data = rom->rom_data;

    mapper.nametable_layout = get_bit(rom_data[6], 0) == 0 ? NT_LAYOUT_VERTICAL : NT_LAYOUT_HORIZONTAL;
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

    // NOTE: This could be made into a hash map sort of deal
    switch (mapper.id) {
        // NROM
        case 0:
            mapper.cb = (MapperCallbacks) {
                .cpu_read  = mapper_nrom_cpu_read,
                .cpu_write = mapper_nrom_cpu_write,
                .ppu_read  = mapper_nrom_ppu_read,
                .ppu_write = mapper_nrom_ppu_write,
            };
            break;
        // MMC1
        case 1:
            mapper.cb = (MapperCallbacks) {
                .cpu_read  = mapper_mmc1_cpu_read,
                .cpu_write = mapper_mmc1_cpu_write,
                .ppu_read  = mapper_mmc1_ppu_read,
                .ppu_write = mapper_mmc1_ppu_write,
            };
            mapper.prg_rom_bank0 = mapper.prg_rom;
            mapper.prg_rom_bank1 = mapper.prg_rom + mapper.prg_rom_size - kib(16);

            mapper.chr_rom_bank0 = mapper.chr_rom;
            mapper.chr_rom_bank1 = mapper.chr_rom + kib(4);
            break;
        default:
            log_fatal("Unsupported mapper: mapper %u, submapper %u", mapper.id, mapper.submapper);
            UNREACHABLE();
    }

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
