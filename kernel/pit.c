#include "pit.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"
#include "../sched/sched.h"

// Preempt the running task every PIT_SLICE ticks. At 100 Hz this is a 100 ms
// round-robin time slice.
#define PIT_SLICE 10

#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43
// Channel 0, lo/hi byte access, mode 3 (square wave), binary: 0x36
#define PIT_MODE      0x36
#define PIT_BASE_HZ   1193182

static volatile uint32_t ticks = 0;

static void pit_tick(registers_t *r) {
    (void)r;
    ticks++;
    // Preemptive round robin: hand the CPU to the next ready task. schedule()
    // is a no-op until the scheduler is up and a second task exists, so this
    // is safe during early boot.
    if ((ticks % PIT_SLICE) == 0) schedule();
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
