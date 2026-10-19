#include "nes/common.h"

#include "nes/ppu.h"

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

i32 main(void) {
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_CHECK(SDL_CreateWindowAndRenderer("nesemu", 1280, 720, 0, &window, &renderer));
    // SDL_SetWindowResizable(window, true);

    Ppu ppu = ppu_init();
    ppu__debug_fill_pixels(&ppu);

    for (i32 luma = 0; luma < 4; luma++) {
        for (i32 chroma = 0; chroma < 16; chroma++) {
            u8 color = (luma << 4) | chroma;
            const u8 pixel_size = 8;
            Color decoded = ppu_decode_color(&ppu, color);
            for (u32 dy = 0; dy < pixel_size; dy++) {
                for (u32 dx = 0; dx < pixel_size; dx++) {
                    u32 x = LEFT_BORDER_WIDTH + dx + chroma*pixel_size;
                    u32 y = dy + luma*pixel_size;
                    ((Color*) ppu.pixels)[x+y*OUTPUT_WIDTH] = decoded;
                }
            }
        }
    }

    SDL_Texture* output = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, OUTPUT_WIDTH, OUTPUT_HEIGHT);
    SDL_SetTextureScaleMode(output, SDL_SCALEMODE_PIXELART);

    b8 running = true;
    b8 draw_border = false;
    b8 integer_scaling = false;

    while (running) {
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

        // Update with latest ppu output
        u8* pixels = NULL;
        i32 pitch = -1;
        if (SDL_LockTexture(output, NULL, (void**) &pixels, &pitch)) {
            for (i32 y = 0; y < OUTPUT_HEIGHT; y++) {
                for (i32 x = 0; x < OUTPUT_WIDTH; x++) {
                    i32 idx = x+y*OUTPUT_WIDTH;
                    pixels[idx*4 + 0] = ppu.pixels[idx*3 + 0];
                    pixels[idx*4 + 1] = ppu.pixels[idx*3 + 1];
                    pixels[idx*4 + 2] = ppu.pixels[idx*3 + 2];
                    pixels[idx*4 + 3] = 255;
                }
            }
            SDL_UnlockTexture(output);
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
        SDL_CHECK(SDL_RenderTexture(renderer, output, &src, &dst));

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
