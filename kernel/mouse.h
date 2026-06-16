#pragma once
#include "../include/types.h"

#define MOUSE_SCREEN_W 80
#define MOUSE_SCREEN_H 25

typedef struct {
    int32_t x;       // accumulated X position
    int32_t y;       // accumulated Y position
    uint8_t buttons; // bit0=left, bit1=right, bit2=middle
} mouse_state_t;

void mouse_init(void);
mouse_state_t mouse_get_state(void);
