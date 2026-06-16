#include "pit.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"

#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43
// Channel 0, lo/hi byte access, mode 3 (square wave), binary: 0x36
#define PIT_MODE      0x36
#define PIT_BASE_HZ   1193182

static volatile uint32_t ticks = 0;

static void pit_tick(registers_t *r) {
    (void)r;
    ticks++;
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_HZ / hz;

    outb(PIT_CMD, PIT_MODE);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, pit_tick);
    pic_clear_mask(0);   // unmask IRQ0
}

uint32_t pit_get_ticks(void) {
    return ticks;
}
