#include "nes/common.h"

#include "nes/cpu.h"
#include "nes/ppu.h"
#include "nes/dma.h"
#include "nes/rom.h"
#include "nes/log.h"
#include "nes/input.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <SDL3/SDL.h>
// #include <glad/gl.h>

#define SDL_CHECK(func) do { \
    if (!func) { \
        fprintf(stderr, "ERR: "#func": %s\n", SDL_GetError()); \
        exit(1); \
    } \
} while (0)

typedef struct Machine Machine;
struct Machine {
    u8 cpu_ram[0x800];
    NesRom rom;
    Cpu cpu;
    Ppu ppu;
    OamDma oam_dma;
    InputDevice input;
};

u8 mem_cpu_read(MemoryBus* bus, u16 address) {
    Machine* machine = bus->ctx;
    // log_debug("READ: "HEX_U16, address);
    if (address < 0x2000) {
        return machine->cpu_ram[address & 0x7FF];
    } else if (address >= 0x2000 && address < 0x4000) {
        // PPU registers
        u8 reg = address & 0x7;
        Ppu* ppu = &machine->ppu;
        return ppu_mmio_read(ppu, reg);
    } else if (address >= 0x4000 && address < 0x4018) {
        switch (address) {
            // Controller 1
            case 0x4016: {
                u8 value = input_device_read(&machine->input);
                // Preserve top 3 bits of open bus
                value |= ((machine->cpu.data_bus >> 5) << 5);
                return value;
            }
            case 0x4017:
                // TODO: Implement a second controller
                return (machine->cpu.data_bus >> 5) << 5;
            default:
                return machine->cpu.data_bus;
        }
    } else if (address >= 0x4018 && address < 0x4020) {
        log_warn("Read to "HEX_U16" which is only avilable in CPU test mode!", address);
        return machine->cpu.data_bus;
    } else {
        i16 memory = machine->rom.mapper.cb.cpu_read(&machine->rom.mapper, address);
        // Unmapped
        if (memory == -1) {
            return machine->cpu.data_bus;
        } else {
            return memory;
        }
    }
}

void mem_cpu_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    if (address < 0x2000) {
        machine->cpu_ram[address & 0x7FF] = value;
    } else if (address >= 0x2000 && address < 0x4000) {
        // PPU registers
        u8 reg = address & 0x7;
        Ppu* ppu = &machine->ppu;
        ppu_mmio_write(ppu, reg, value);
    } else if (address >= 0x4000 && address < 0x4018) {
        switch (address) {
            case 0x4014:
                oam_dma_signal(&machine->oam_dma, value);
                break;
            case 0x4016:
                machine->input.polling = value & 0x1;
                break;
            default:
                break;
        }
    } else if (address >= 0x4018 && address < 0x4020) {
        log_warn("Read to "HEX_U16" which is only avilable in CPU test mode!", address);
    } else {
        machine->rom.mapper.cb.cpu_write(&machine->rom.mapper, address, value);
    }
}

u8 mem_ppu_read(MemoryBus* bus, u16 address) {
    Machine* machine = bus->ctx;
    Mapper* mapper = &machine->rom.mapper;
    return mapper->cb.ppu_read(mapper, address);
}

void mem_ppu_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    Mapper* mapper = &machine->rom.mapper;
    mapper->cb.ppu_write(mapper, address, value);
}

f32 timestamp(void) { return SDL_GetTicks() / 1e-3f; }

i32 main(void) {
    logger_register_callback(log_stdout_callback, NULL);

#if 1

    //
    // Initialize NES
    //
    Machine* machine = malloc(sizeof(Machine));
    *machine = (Machine) {
        // .rom = load_nes_rom_from_file("nes_roms/Donkey Kong (World) (Rev 1).nes"),
        .rom = load_nes_rom_from_file("nes_roms/Super Mario Bros. (World).nes"),
        // .rom = load_nes_rom_from_file("nes_roms/Tetris (USA).nes"),
        // .rom = load_nes_rom_from_file("nes_roms/Super Mario Bros. 3 (USA).nes"), // Needs mapper 4.0
        // .rom = load_nes_rom_from_file("nes_roms/Galaga - Demons of Death (USA).nes"),
        // .rom = load_nes_rom_from_file("nes_roms/Ice Climber (USA, Europe, Korea) (En).nes"),

        // .rom = load_nes_rom_from_file("nes_roms/tests/color_test.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/full_nes_palette.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/palette.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/demo_ntsc.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/ppu_vbl_nmi.nes"), // Needs unofficial opcodes

        // .rom = load_nes_rom_from_file("nes_roms/tests/AccuracyCoin.nes"),

        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_overflow/3.Timing.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_overflow/5.Emulator.nes"),

        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_dummy_reads.nes"), // Needs mapper 3.0
        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_dummy_writes_oam.nes"), // Needs unofficial opcodes
        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_dummy_writes_ppumem.nes"), // Needs unofficial opcodes
        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_interrupts.nes"), // Needs unofficial opcodes
        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_reset_registers.nes"),

        // =====================================================================
        // Passing
        // =====================================================================
        // .rom = load_nes_rom_from_file("nes_roms/tests/1.Branch_Basics.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/2.Backward_Branch.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/3.Forward_Branch.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/cpu_timing_test.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/ppu_open_bus.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_overflow/1.Basics.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_overflow/2.Details.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_overflow/4.Obscure.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/01.basics.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/02.alignment.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/03.corners.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/04.flip.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/05.left_clip.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/06.right_edge.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/07.screen_bottom.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/08.double_height.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/09.timing_basics.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/10.timing_order.nes"),
        // .rom = load_nes_rom_from_file("nes_roms/tests/sprite_hit_tests_2005.10.05/11.edge_timing.nes"),

        .input = input_device_keyboard(),
    };
    print_nes_rom(machine->rom);

    // Currently hard-coded to be an NMOS mapper.
    machine->cpu = cpu_init((MemoryBus) {
            .ctx = machine,
            .read = mem_cpu_read,
            .write = mem_cpu_write,
        });
    Cpu* cpu = &machine->cpu;
    cpu->disable_decimal_mode = true;

    machine->ppu = ppu_init(timestamp, (MemoryBus) {
            .ctx = machine,
            .read = mem_ppu_read,
            .write = mem_ppu_write,
        });
    Ppu* ppu = &machine->ppu;

    //
    // Initialize other stuff
    //
    SDL_CHECK(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_CHECK(SDL_CreateWindowAndRenderer("nesemu", 1280, 720, 0, &window, &renderer));
    assert(window != NULL);
    assert(renderer != NULL);
    // SDL_SetWindowResizable(window, true);

    u8 output_pixels[PICTURE_WIDTH*PICTURE_HEIGHT*4] = {0};
    SDL_Texture* output = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, PICTURE_WIDTH, PICTURE_HEIGHT);
    SDL_SetTextureScaleMode(output, SDL_SCALEMODE_PIXELART);

    b8 running = true;
    b8 integer_scaling = false;

    b8 is_dirty = true;
    u32 pixel_idx = 0;
    u64 clock_hz = 21441960;
    u64 clock_timer = 0;
    u64 tick = 0;

    u64 last = SDL_GetTicksNS();
    while (running) {
        u64 now = SDL_GetTicksNS();
        u64 dt_ns = now - last;
        last = now;

        clock_timer += dt_ns;
        while (clock_timer >= 1.0e9 / clock_hz) {
            if (tick % 12 == 0) {
                machine->input.update(&machine->input);
                cpu_step(cpu);
                // log_debug("%lu", cpu->cycle);
                oam_dma_step(&machine->oam_dma, cpu, ppu);
            }

            if (tick % 4 == 0) {
                PpuDot dot = ppu_step(ppu);

                if (dot.visible) {
                    // output_pixels[(dot.x + dot.y*PICTURE_WIDTH)*4 + 0] = dot.color.r;
                    // output_pixels[(dot.x + dot.y*PICTURE_WIDTH)*4 + 1] = dot.color.g;
                    // output_pixels[(dot.x + dot.y*PICTURE_WIDTH)*4 + 2] = dot.color.b;
                    // output_pixels[(dot.x + dot.y*PICTURE_WIDTH)*4 + 3] = 255;

                    // log_debug("$%04X", ppu->v);
                    output_pixels[pixel_idx++] = dot.color.r;
                    output_pixels[pixel_idx++] = dot.color.g;
                    output_pixels[pixel_idx++] = dot.color.b;
                    output_pixels[pixel_idx++] = 255;
                }
            }

            cpu->nmi = ppu->vblank_nmi_signal;

            if (ppu->vblank_flag && is_dirty) {
                assert(pixel_idx == PICTURE_WIDTH*PICTURE_HEIGHT*4);
                is_dirty = false;
                pixel_idx = 0;
                SDL_UpdateTexture(output, NULL, output_pixels, PICTURE_WIDTH*4);
            } else if (!ppu->vblank_flag) {
                is_dirty = true;
            }

            tick++;
            clock_timer -= 1e9 / clock_hz;
        }

        // Update visualizer
        SDL_Event ev = {0};
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    running = false;
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Rect viewport = {0};
        SDL_CHECK(SDL_GetRenderViewport(renderer, &viewport));

        SDL_FRect src = {
            .x = 0,
            .y = 0,
            .w = PICTURE_WIDTH,
            .h = PICTURE_HEIGHT,
        };

        f32 x_scale = viewport.w / src.w;
        f32 y_scale = viewport.h / src.h;
        f32 scale = min(x_scale, y_scale);
        if (integer_scaling) {
            scale = floorf(scale);
        }
        // scale = 1;

        SDL_FRect dst = {
            .x = roundf((viewport.w - src.w*scale) / 2.0f),
            .y = roundf((viewport.h - src.h*scale) / 2.0f),
            .w = src.w*scale,
            .h = src.h*scale,
        };
        SDL_RenderTexture(renderer, output, &src, &dst);

        // SDL_RenderTexture(renderer, pattern_table0, NULL, &(SDL_FRect) {
        //         .x = 8,
        //         .y = 8,
        //         .w = 256,
        //         .h = 256,
        //     });

        SDL_RenderPresent(renderer);
    }

    free(machine->rom.rom_data.data);
    free(machine);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;

#elif 0

    // TODO: Pull this out into its own test file.

    u8 prg_rom[16384] = {0};
    u8 chr_rom[8192] = {0};
    memset(prg_rom, 0xEA, sizeof(prg_rom));

    Machine machine = {
        .rom = {
            .prg_rom = prg_rom,
            .prg_rom_size = sizeof(prg_rom),
            .chr_rom = chr_rom,
            .chr_rom_size = 8192,
        },
    };
    Cpu* cpu = &machine.cpu;
    Ppu* ppu = &machine.ppu;
    MemoryBus cpu_mem_bus = {
        .ctx = &machine,
        .read = mem_cpu_read,
        .write = mem_cpu_write,
    };
    MemoryBus ppu_mem_bus = {
        .ctx = &machine,
        .read = mem_ppu_read,
        .write = mem_ppu_write,
    };

    // =========================================================================
    // Test cycle count for a single scanline (rendering disabled)

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);
    u32 counter = 0;
    while (ppu->scanline == -1) {
        if (counter % 3 == 0) {
            cpu_step(cpu);
        }
        ppu_step(ppu);
        counter++;
    }
    assert(ppu->cycle == 341);
    assert(cpu->cycle == 114);

    // =========================================================================
    // Test cycle count for a single frame (rendering disabled)

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);
    counter = 0;
    while (counter == 0 || ppu->scanline != -1 || ppu->dot != 0) {
        if (counter % 3 == 0) {
            cpu_step(cpu);
        }
        ppu_step(ppu);
        counter++;
    }
    assert(ppu->cycle == 89342);
    assert(cpu->cycle == 29781);

    // =========================================================================
    // Test cycle count for three frames (rendering disabled)
    //
    // Frames have to be averaged over a number of frames divisable by three
    // or else the timing won't be correct because the CPU and PPU won't be
    // synced by the end.

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);
    counter = 0;
    u32 frame_counter = 0;
    while (frame_counter < 3) {
        if (counter % 3 == 0) {
            cpu_step(cpu);
        }
        ppu_step(ppu);

        if (counter > 0 && ppu->scanline == -1 && ppu->dot == 0) {
            frame_counter++;
        }
        counter++;
    }
    const f64 theta = 1e-3;
    assert(ppu->cycle / 3.0 == 89342.0);
    assert(cpu->cycle / 3.0 >= 29780.667 - theta &&
            cpu->cycle / 3.0 <= 29780.667 + theta);

    // =========================================================================
    // Test cycle count for two frames (rendering enabled)

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);
    ppu->mask.background = 1;
    ppu->mask.sprite = 1;
    counter = 0;
    frame_counter = 0;
    while (frame_counter < 2) {
        if (counter % 3 == 0) {
            cpu_step(cpu);
        }
        ppu_step(ppu);

        if (counter > 0 && ppu->scanline == -1 && ppu->dot == 0) {
            frame_counter++;
        }
        counter++;
    }
    assert(ppu->cycle / 2.0 == 89341.5);
    assert(cpu->cycle / 2.0 == 29780.5);

    // =========================================================================
    // Test V-blank duration (rendering disabled)

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);
    counter = 0;
    i32 vblank_start = -1;
    while (vblank_start == -1 || ppu->vblank_flag) {
        if (vblank_start == -1 && ppu->vblank_flag) {
            vblank_start = ppu->cycle;
        }

        if (counter % 3 == 0) {
            cpu_step(cpu);
        }
        ppu_step(ppu);
        counter++;
    }
    assert((ppu->cycle - vblank_start) / 341.0 == 20.0);

#elif 0

    u8 prg_rom[16384] = {0};
    u8 chr_rom[8192] = {0};
    memset(prg_rom, 0xEA, sizeof(prg_rom));

    Machine machine = {
        .rom = {
            .prg_rom = prg_rom,
            .prg_rom_size = sizeof(prg_rom),
            .chr_rom = chr_rom,
            .chr_rom_size = 8192,
        },
    };
    Cpu* cpu = &machine.cpu;
    Ppu* ppu = &machine.ppu;
    MemoryBus cpu_mem_bus = {
        .ctx = &machine,
        .read = mem_cpu_read,
        .write = mem_cpu_write,
    };
    MemoryBus ppu_mem_bus = {
        .ctx = &machine,
        .read = mem_ppu_read,
        .write = mem_ppu_write,
    };

    *cpu = cpu_init(cpu_mem_bus);
    *ppu = ppu_init(ppu_mem_bus);

    u32 index_to_check = 9;
    ppu->v = 0x3F00 + index_to_check;
    ppu->palette_indices[index_to_check] = 10;

    u8 framebuffer[PICTURE_WIDTH*PICTURE_HEIGHT*3] = {0};
    while (ppu->scanline < 240) {
        PpuDot dot = ppu_step(ppu);
        if (dot.visible) {
            framebuffer[(dot.x+dot.y*PICTURE_WIDTH)*3 + 0] = dot.color.r;
            framebuffer[(dot.x+dot.y*PICTURE_WIDTH)*3 + 1] = dot.color.g;
            framebuffer[(dot.x+dot.y*PICTURE_WIDTH)*3 + 2] = dot.color.b;
        }
    }

    u8 expected_framebuffer[PICTURE_WIDTH*PICTURE_HEIGHT*3] = {0};
    Color c = ppu->palette[ppu->palette_indices[index_to_check]];
    for (u32 i = 0; i < PICTURE_WIDTH*PICTURE_HEIGHT; i++) {
        expected_framebuffer[i*3+0] = c.r;
        expected_framebuffer[i*3+1] = c.g;
        expected_framebuffer[i*3+2] = c.b;
    }

    assert(memcmp(framebuffer, expected_framebuffer, sizeof(framebuffer)) == 0);

#endif
}
