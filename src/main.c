#include "nes/common.h"

#include "nes/cpu.h"
#include "nes/ppu.h"
#include "nes/rom.h"
#include "nes/opcode_table.h"

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
    u8 ppu_vram[0x800];
    NesRom rom;
    Cpu cpu;
    Ppu ppu;
};

u8* mem_cpu_helper(Machine* machine, u16 address) {
    if (address < 0x2000) {
        return &machine->cpu_ram[address & 0x07FF];
    }

    if (address >= 0x2000 && address < 0x4000) {
        return NULL;
    }

    if (address >= 0x4000 && address < 0x4018) {
        static u8 dummy = 0xFE;
        return &dummy;
    }

    // PRG-RAM
    if (address >= 0x6000 && address < 0x8000) {
        return NULL;
    }

    if (address >= 0x8000 && address < 0xC000) {
        // printf("%d\n", address - 0x8000 >= machine->rom.prg_rom_size);
        return &machine->rom.prg_rom[address - 0x8000];
    }

    if (address >= 0xC000) {
        if (machine->rom.prg_rom_size > kib(16)) {
            return &machine->rom.prg_rom[address - 0x8000];
        } else {
            return &machine->rom.prg_rom[address - 0xC000];
        }
    }

    return NULL;
}

u8 mem_cpu_read(MemoryBus* bus, u16 address) {
    Machine* machine = bus->ctx;
    const u8 dummy = 0xFE;
    const u8* memory = mem_cpu_helper(machine, address);
    u8 value = dummy;
    if (address >= 0x2000 && address < 0x4000) {
        Ppu* ppu = &machine->ppu;
        u8 ppu_op = address & 7;
        switch (ppu_op) {
            case 0:
                UNREACHABLE();
            case 1:
                UNREACHABLE();
            case 2:
                value = ppu_status(ppu);
                break;
            case 3:
                UNREACHABLE();
            case 4:
                value = ppu_oam_data_read(ppu);
                break;
            case 5:
                UNREACHABLE();
            case 6:
                UNREACHABLE();
            case 7:
                value = ppu_data_read(ppu);
                break;
        }
    } else {
        if (memory == NULL) {
            memory = &dummy;
        }
        value = *memory;
    }

    // printf("CPU: READ(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, value, value);

    if (memory == &dummy) {
        abort();
    }
    return value;
}

void mem_cpu_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    if (address >= 0x2000 && address < 0x4000) {
        Ppu* ppu = &machine->ppu;
        u8 ppu_op = address & 7;
        switch (ppu_op) {
            case 0:
                ppu_ctrl(ppu, value);
                break;
            case 1:
                ppu_mask(ppu, value);
                break;
            case 2:
                // TODO: Whatever writing to a read only register is supposed to
                // do.
                UNREACHABLE();
                break;
            case 3:
                ppu_oam_address(ppu, value);
                break;
            case 4:
                ppu_oam_data_write(ppu, value);
                break;
            case 5:
                ppu_scroll(ppu, value);
                break;
            case 6:
                ppu_address(ppu, value);
                break;
            case 7:
                ppu_data_write(ppu, value);
                break;
        }
    } else {
        u8* memory = mem_cpu_helper(machine, address);
        *memory = value;
    }

    if (address >= 0x8000) {
        fprintf(stderr, "WARN: Writing to ROM!");
        // abort();
    }

    // printf("CPU: WRITE(t%d): $%04X, %d ($%02X)\n", machine->cpu.t, address, value, value);
}

u8* mem_ppu_helper(Machine* machine, u16 address) {
    if (address < 0x2000) {
        // printf("CHR-ROM access\n");
        return &machine->rom.chr_rom[address];
    }

    // Nametable and attribute tables
    if (address >= 0x2000 && address < 0x3000) {
        // Vert 0 => $2000 $2400 $2000 $2400
        // Hori 1 => $2000 $2000 $2800 $2800

        // NOTE: Don't ask me how I came up with this bit magic. It took me half an
        // hour at midnight to come up with this one-liner instead of like two
        // if statements.
        address &= ~(0x800 >> machine->rom.nametable_layout);

        return &machine->ppu_vram[address - 0x2000];
    }

    if (address >= 0x3000 && address < 0x3F00) {
        return mem_ppu_helper(machine, address - 0x1000);
    }

    // Internal palette
    if (address >= 0x3F00 && address < 0x4000) {
        // Wrap to 32 bit index
        u16 index = address & 0x1F;

        // Sprite color 0 mirrors background color 0 in every palette
        b8 is_sprite = index & 0x10;
        if (is_sprite != 0 && (index & 0x3) == 0) {
            index -= 0x10;
        }

        return &machine->ppu.palette_indices[index];
    }

    return NULL;
}

u8 mem_ppu_read(MemoryBus* bus, u16 address) {
    Machine* machine = bus->ctx;
    u8* memory = mem_ppu_helper(machine, address);
    u8 value = 0xFE;
    if (memory != NULL) {
        value = *memory;
    }
    // printf("PPU: READ(%d:%d): $%04X, %d ($%02X)\n", machine->ppu.scanline, machine->ppu.dot, address, value, value);
    if (memory == NULL) {
        abort();
    }
    return value;
}

void mem_ppu_write(MemoryBus* bus, u16 address, u8 value) {
    Machine* machine = bus->ctx;
    // printf("PPU: WRITE(%d:%d): $%04X, %d ($%02X)\n", machine->ppu.scanline, machine->ppu.dot, address, value, value);
    *mem_ppu_helper(machine, address) = value;
}

u16 step_instruction(Machine* machine) {
    Cpu* cpu = &machine->cpu;
    assert(cpu->t == 0);

    u16 start_address = cpu->pc;

    printf("$%04X:    %s", cpu->pc, OPCODE_STRING_TABLE[*mem_cpu_helper(machine, cpu->pc)]);
    switch (cpu->interrupt_type) {
        case NONE:
            break;
        case NMI:
            printf(" (NMI)");
            break;
        case RESET:
            printf(" (RESET)");
            break;
        case IRQ:
            printf(" (IRQ)");
            break;
        case BRK:
            printf(" (BRK)");
            break;
        default:
            UNREACHABLE();
    }
    printf("\n");

    cpu_step(cpu);
    while (cpu->t != 0) {
        u8 curr_t = cpu->t;
        cpu_step(cpu);
        if (curr_t == cpu->t) {
            exit(1);
        }
    }

    print_cpu_status(cpu->p);
    printf("p = %d (0x%02X)\n", cpu->p, cpu->p);
    printf("a = %d (0x%02X)\n", cpu->a, cpu->a);
    printf("x = %d (0x%02X)\n", cpu->x, cpu->x);
    printf("s = %d (0x%02X)\n", cpu->s, cpu->s);
    printf("\n");

    return start_address;
}

void print_bits(const void* data, u32 byte_count) {
    for (u32 i = 0; i < byte_count; i++) {
        u8 value = ((const u8*) data)[(byte_count-1) - i];
        for (u32 j = 0; j < 8; j++) {
            printf("%d", (value >> (7-j)) & 0x1);
        }
        if (i < byte_count-1) {
            printf(" ");
        }
    }
    printf("\n");
}

i32 main(void) {
    // u8 rom[0x8000] = {0};
    // // NMI vector
    // rom[0xFFFA - 0x8000] = 0x00;
    // rom[0xFFFB - 0x8000] = 0x90;
    // // Reset vector
    // rom[0xFFFC - 0x8000] = 0x00;
    // rom[0xFFFD - 0x8000] = 0x80;
    // // IRQ vector
    // rom[0xFFFE - 0x8000] = 0x00;
    // rom[0xFFFF - 0x8000] = 0x81;
    //
    // // NMI routine
    // // NOP
    // rom[0x1000] = 0xEA;
    // // RTI
    // rom[0x1001] = 0x40;
    //
    // // BRK
    // rom[0x000] = 0x00;
    // rom[0x001] = 0xEA;
    // // LDA #imm
    // rom[0x002] = 0xA9;
    // rom[0x003] = 0xEA;
    // // JSR abs
    // rom[0x004] = 0x20;
    // rom[0x005] = 0x00;
    // rom[0x006] = 0x82;
    // // NOP
    // rom[0x007] = 0xEA;
    //
    // // IRQ routine
    // // NOP
    // rom[0x100] = 0xEA;
    // // RTI
    // rom[0x101] = 0x40;
    //
    // // Subroutine target
    // // NOP
    // rom[0x200] = 0xEA;
    // // RTS
    // rom[0x201] = 0x60;
    //
    // Machine m = {
    //     .rom = (NesRom) {
    //         .prg_rom_size = sizeof(rom),
    //         .prg_rom = rom,
    //     },
    // };
    // m.cpu = cpu_init((MemoryBus) {
    //         .ctx = &m,
    //         .read = mem_cpu_read,
    //         .write = mem_cpu_write,
    //     });
    // m.cpu.s = 0xFF;
    //
    // // Init
    // step_instruction(&m);
    //
    // // BRK
    // step_instruction(&m);
    // // IRQ routine
    // step_instruction(&m);
    // step_instruction(&m);
    // // LDA
    // m.cpu.nmi = 1;
    // step_instruction(&m);
    // // JSR (NMI)
    // step_instruction(&m);
    //
    // // NMI routine
    // step_instruction(&m);
    // step_instruction(&m);
    // step_instruction(&m);
    //
    // // Subroutine
    // step_instruction(&m);
    // step_instruction(&m);
    // // NOP
    // step_instruction(&m);
    //
    // return 0;

    //
    // Initialize NES
    //
    Machine machine = {
        // .rom = load_nes_rom_from_file("nes_roms/Tetris (USA).nes"),
        // .rom = load_nes_rom_from_file("nes_roms/Super Mario Bros. (World).nes"),
        .rom = load_nes_rom_from_file("nes_roms/Donkey Kong (World) (Rev 1).nes"),
        // .rom = load_nes_rom_from_file("nes_roms/color_test.nes"),
    };
    print_nes_rom(machine.rom);
    assert(machine.rom.mapper == 0 && machine.rom.submapper == 0);

    // Currently hard-coded to be an NMOS mapper.
    machine.cpu = cpu_init((MemoryBus) {
            .ctx = &machine,
            .read = mem_cpu_read,
            .write = mem_cpu_write,
        });
    Cpu* cpu = &machine.cpu;

    // cpu->bus.read(&cpu->bus, 0x8E3E);

    machine.ppu = ppu_init((MemoryBus) {
            .ctx = &machine,
            .read = mem_ppu_read,
            .write = mem_ppu_write,
        });
    Ppu* ppu = &machine.ppu;
    ppu__debug_fill_pixels(ppu, 5);
    machine.rom.nametable_layout = NT_LAYOUT_VERTICAL;

    // for (u32 i = 0; i < 5e5; i++) {
    //     if (cpu->t == 0 && cpu->interrupt_type != NONE) {
    //         printf("$%04X:    %s", cpu->pc, OPCODE_STRING_TABLE[*mem_cpu_helper(&machine, cpu->pc)]);
    //         switch (cpu->interrupt_type) {
    //             case NONE:
    //                 break;
    //             case NMI:
    //                 printf(" (NMI)");
    //                 break;
    //             case RESET:
    //                 printf(" (RESET)");
    //                 break;
    //             case IRQ:
    //                 printf(" (IRQ)");
    //                 break;
    //             case BRK:
    //                 printf(" (BRK)");
    //                 break;
    //             default:
    //                 UNREACHABLE();
    //         }
    //         printf("\n");
    //     }
    //     cpu_step(cpu);
    //
    //     ppu_step(ppu);
    //     ppu_step(ppu);
    //     ppu_step(ppu);
    //
    //     cpu->nmi = ppu->vblank_nmi_signal;
    // }
    //
    // return 0;

    //
    // Initialize other stuff
    //
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, &(char) {1});
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_CHECK(SDL_CreateWindowAndRenderer("nesemu", 1280, 720, 0, &window, &renderer));
    // SDL_SetWindowResizable(window, true);

    SDL_Texture* output = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, OUTPUT_WIDTH, OUTPUT_HEIGHT);
    SDL_SetTextureScaleMode(output, SDL_SCALEMODE_PIXELART);

    {
        u8* pixels = NULL;
        i32 pitch = -1;
        if (SDL_LockTexture(output, NULL, (void**) &pixels, &pitch)) {
            for (u32 i = 0; i < OUTPUT_WIDTH*OUTPUT_HEIGHT; i++) {
                Color color = ppu_decode_color(ppu, ppu->pixels[i]);
                pixels[i*4+0] = color.r;
                pixels[i*4+1] = color.g;
                pixels[i*4+2] = color.b;
                pixels[i*4+3] = 255;
            }
            SDL_UnlockTexture(output);
        }
    }

    // Tiles in pattern table = 16*16
    // Tile size = 8*8 pixels
    #define TILE_SIZE 8
    #define PATTERN_SIZE 16
    SDL_Texture* pattern_table0 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, PATTERN_SIZE*TILE_SIZE, PATTERN_SIZE*TILE_SIZE);
    u8 pixels[PATTERN_SIZE*PATTERN_SIZE * TILE_SIZE*TILE_SIZE * 4] = {0};
    const u8* pattern_data = machine.rom.chr_rom;
    for (u32 tile_y = 0; tile_y < PATTERN_SIZE; tile_y++) {
        for (u32 tile_x = 0; tile_x < PATTERN_SIZE; tile_x++) {
            u32 tile_idx = (tile_y << 4) | tile_x;
            for (u32 py = 0; py < 8; py++) {
                u8 plane0 = pattern_data[0x0000 + tile_idx*16 + py];
                u8 plane1 = pattern_data[0x0000 + tile_idx*16 + 8 + py];
                for (u32 px = 0; px < 8; px++) {
                    u8 color = get_bit(plane0, 7-px) | (get_bit(plane1, 7-px) << 1);
                    color = (u8[]) {
                        0,
                        64,
                        128,
                        255,
                    }[color];

                    u32 x = tile_x*TILE_SIZE + px;
                    u32 y = tile_y*TILE_SIZE + py;
                    u32 pixel_idx = x + y*PATTERN_SIZE*TILE_SIZE;

                    pixels[pixel_idx*4 + 0] = color;
                    pixels[pixel_idx*4 + 1] = color;
                    pixels[pixel_idx*4 + 2] = color;
                    pixels[pixel_idx*4 + 3] = 0xFF;
                }
            }

        }
    }

    SDL_UpdateTexture(pattern_table0, &(SDL_Rect) {
            .x = 0,
            .y = 0,
            .w = 16*8,
            .h = 16*8,
        }, pixels, 16*8*4);
    SDL_SetTextureScaleMode(pattern_table0, SDL_SCALEMODE_PIXELART);

    b8 running = true;
    b8 draw_border = true;
    b8 integer_scaling = false;

    b8 is_dirty = true;

    u64 clock_hz = 21441960;
    u64 clock_timer = 0;
    u32 tick = 0;

    u64 last = SDL_GetTicksNS();
    while (running) {
        u64 now = SDL_GetTicksNS();
        u64 dt_ns = now - last;
        last = now;

        clock_timer += dt_ns;
        while (clock_timer >= 1.0e9 / clock_hz) {
            if (tick % 12 == 0) {
                cpu_step(cpu);
            }

            if (tick % 4 == 0) {
                ppu_step(ppu);
            }

            cpu->nmi = ppu->vblank_nmi_signal;

            if (ppu->vblank_flag && is_dirty) {
                // printf("VBlanking\n");
                is_dirty = false;
                // printf("%lu\n", cpu->cycle);
                // printf("VBlank (%d:%d)\n", ppu->scanline, ppu->dot);

                u8* pixels = NULL;
                i32 pitch = -1;
                if (SDL_LockTexture(output, NULL, (void**) &pixels, &pitch)) {
                    for (u32 i = 0; i < OUTPUT_WIDTH*OUTPUT_HEIGHT; i++) {
                        Color color = ppu_decode_color(ppu, ppu->pixels[i]);
                        pixels[i*4+0] = color.r;
                        pixels[i*4+1] = color.g;
                        pixels[i*4+2] = color.b;
                        pixels[i*4+3] = 255;
                    }
                    SDL_UnlockTexture(output);
                }
            } else if (!ppu->vblank_flag) {
                is_dirty = true;
            }

            tick++;
            clock_timer -= 1e9 / clock_hz;
        }

        // fps++;
        // if (SDL_GetTicksNS() - last_fps >= 1e9) {
        //     printf("FPS: %u\n", fps);
        //     fps = 0;
        //     last_fps = SDL_GetTicksNS();
        // }

            // if (cpu->t == 0 && cpu->pc != 0x8057) {
            //     printf("$%04X:    %s", cpu->pc, OPCODE_STRING_TABLE[*mem_cpu_helper(&machine, cpu->pc)]);
            //     switch (cpu->interrupt_type) {
            //         case NONE:
            //             break;
            //         case NMI:
            //             printf(" (NMI)");
            //             break;
            //         case RESET:
            //             printf(" (RESET)");
            //             break;
            //         case IRQ:
            //             printf(" (IRQ)");
            //             break;
            //         case BRK:
            //             printf(" (BRK)");
            //             break;
            //     }
            //     printf("\n");
            // }

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
            .x = LEFT_BORDER_WIDTH,
            .y = 0,
            .w = PICTURE_WIDTH,
            .h = PICTURE_HEIGHT,
        };

        if (draw_border) {
            src = (SDL_FRect) {
                .x = 0,
                .y = 0,
                .w = OUTPUT_WIDTH,
                .h = OUTPUT_HEIGHT,
            };
        }

        f32 x_scale = viewport.w / src.w;
        f32 y_scale = viewport.h / src.h;
        f32 scale = min(x_scale, y_scale);
        if (integer_scaling) {
            scale = floorf(scale);
        }

        SDL_FRect dst = {
            .x = roundf((viewport.w - src.w*scale) / 2.0f),
            .y = roundf((viewport.h - src.h*scale) / 2.0f),
            .w = src.w*scale,
            .h = src.h*scale,
        };
        SDL_RenderTexture(renderer, output, &src, &dst);

        SDL_RenderTexture(renderer, pattern_table0, NULL, &(SDL_FRect) {
                .x = 8,
                .y = 8,
                .w = 256,
                .h = 256,
            });

        SDL_RenderPresent(renderer);
    }

    free(machine.rom.full_rom);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
