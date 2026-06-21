#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
static volatile uint16_t *const VGA_MEM = (uint16_t *)0xB8000;

static size_t row, col;
static uint8_t color;

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
    row = 0;
    col = 0;
}

void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        if (++row == VGA_HEIGHT) vga_scroll();
        return;
    }
    if (c == '\b') {
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;                 // step back across a line wrap
            col = VGA_WIDTH - 1;
        } else {
            return;                // already at top-left, nothing to erase
        }
        VGA_MEM[row * VGA_WIDTH + col] = vga_entry(' ', color);
        return;
    }
    VGA_MEM[row * VGA_WIDTH + col] = vga_entry(c, color);
    if (++col == VGA_WIDTH) {
        col = 0;
        if (++row == VGA_HEIGHT) vga_scroll();
    }
}

void vga_write(const char *s) {
    for (; *s; s++) vga_putc(*s);
}

void vga_write_uint(uint32_t n) {
    if (n == 0) { vga_putc('0'); return; }
    char tmp[11];
    int i = 0;
    while (n > 0) { tmp[i++] = (char)('0' + (n % 10)); n /= 10; }
    while (i > 0) vga_putc(tmp[--i]);
}

void vga_put_at(uint8_t x, uint8_t y, char c) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;
    VGA_MEM[y * VGA_WIDTH + x] = vga_entry(c, color);
}
