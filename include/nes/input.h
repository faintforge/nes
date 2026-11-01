#ifndef INPUT_H
#define INPUT_H

#include "nes/common.h"

typedef struct InputDevice InputDevice;
struct InputDevice {
    void (*update)(InputDevice* device);
    b8 polling;
    u8 shifter;
};

extern u8 input_device_read(InputDevice* device);
extern InputDevice input_device_keyboard(void);

#endif // INPUT_H
