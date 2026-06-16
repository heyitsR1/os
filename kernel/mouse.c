#include "mouse.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

// Returns 0 on success, -1 on timeout.
static int ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02)) return 0;
    return -1;
}

static int ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01) return 0;
    return -1;
}

static int mouse_write(uint8_t b) {
    if (ps2_wait_write()) return -1;
    outb(PS2_CMD, 0xD4);
    if (ps2_wait_write()) return -1;
    outb(PS2_DATA, b);
    return 0;
}

// Returns byte read, or -1 on timeout.
static int mouse_read(void) {
    if (ps2_wait_read()) return -1;
    return (int)(uint8_t)inb(PS2_DATA);
}

// 3-byte packet accumulation
static uint8_t  packet[3];
static uint8_t  byte_idx = 0;

static volatile mouse_state_t state = { 0, 0, 0 };

static void mouse_handler(registers_t *r) {
    (void)r;
    packet[byte_idx++] = inb(PS2_DATA);

    if (byte_idx < 3) return;   // wait for full packet
    byte_idx = 0;

    uint8_t flags = packet[0];

    // Discard packet if overflow bits are set — values are unreliable.
    if (flags & 0xC0) return;

    // Sign-extend 9-bit deltas (bit 4/5 of flags are the sign bits).
    int32_t dx = (int32_t)packet[1] - ((flags & 0x10) ? 256 : 0);
    int32_t dy = (int32_t)packet[2] - ((flags & 0x20) ? 256 : 0);

    state.x += dx;
    state.y -= dy;   // screen Y grows downward; mouse Y grows upward
    state.buttons = flags & 0x07;
}

void mouse_init(void) {
    // Enable auxiliary PS/2 device.
    if (ps2_wait_write()) return;
    outb(PS2_CMD, 0xA8);

    // Read the current Compaq status byte and set bit 1 (enable aux IRQ).
    if (ps2_wait_write()) return;
    outb(PS2_CMD, 0x20);
    int status_byte = mouse_read();
    if (status_byte < 0) return;
    uint8_t status = (uint8_t)status_byte | 0x02;   // enable IRQ12
    status &= ~0x20;                                 // clear mouse-clock-disable
    if (ps2_wait_write()) return;
    outb(PS2_CMD, 0x60);
    if (ps2_wait_write()) return;
    outb(PS2_DATA, status);

    if (mouse_write(0xF6) < 0) return;   // set defaults
    mouse_read();                          // ACK (ignore timeout here)
    if (mouse_write(0xF4) < 0) return;   // enable data reporting
    mouse_read();                          // ACK

    irq_install_handler(12, mouse_handler);
    pic_clear_mask(2);    // unmask cascade (required for any slave IRQ)
    pic_clear_mask(12);   // unmask IRQ12
}

mouse_state_t mouse_get_state(void) {
    return (mouse_state_t){ state.x, state.y, state.buttons };
}
