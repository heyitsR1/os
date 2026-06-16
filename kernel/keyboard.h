#pragma once
#include "../include/types.h"

void keyboard_init(void);

// Returns the next ASCII character from the ring buffer, or 0 if empty.
char keyboard_getc(void);
