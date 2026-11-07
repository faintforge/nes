#include "nes/input.h"

#include <SDL3/SDL.h>

u8 input_device_read(InputDevice* device) {
    u8 result = device->shifter & 0x1;
    device->shifter >>= 1;
    return result;
}

static void input_device__keyboard_update(InputDevice* device) {
    if (device->polling) {
        i32 key_count = 0;
        const bool* keys = SDL_GetKeyboardState(&key_count);

        device->shifter = 0;

        // Order described here: https://www.nesdev.org/wiki/Standard_controller
        // Right
        device->shifter |= keys[SDL_SCANCODE_RIGHT];
        // Left
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_LEFT];
        // Down
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_DOWN];
        // Up
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_UP];
        // Start
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_R];
        // Select
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_E];
        // B
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_W];
        // A
        device->shifter <<= 1;
        device->shifter |= keys[SDL_SCANCODE_Q];
    }
}

InputDevice input_device_keyboard(void) {
    InputDevice device = {
        .update = input_device__keyboard_update,
        .polling = false,
        .shifter = 0,
    };
    return device;
}
