#pragma once
#include "../include/types.h"

// VGA text-mode colors (low nibble = foreground, high nibble = background).
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE = 1, VGA_GREEN = 2, VGA_CYAN = 3,
    VGA_RED = 4, VGA_MAGENTA = 5, VGA_BROWN = 6, VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8, VGA_LIGHT_BLUE = 9, VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11, VGA_LIGHT_RED = 12, VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14, VGA_WHITE = 15,
};

void vga_init(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_putc(char c);
void vga_write(const char *s);
void vga_clear(void);                           // blank the screen, cursor to (0,0)
void vga_write_uint(uint32_t n);                // decimal, no leading zeros
void vga_put_at(uint8_t x, uint8_t y, char c); // write one char at a cell, no cursor move
