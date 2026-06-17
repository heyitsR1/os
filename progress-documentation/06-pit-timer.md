# 06 — PIT timer

The PIT (Programmable Interval Timer) fires IRQ0 at a fixed rate. We set it to
100 Hz — 100 ticks per second, one every 10 ms. This is the kernel's heartbeat
and eventually what drives preemptive task switching.

## Why you need a timer

Without a hardware timer the kernel can't take back control from a running task.
If something loops forever, the CPU is stuck. The PIT solves this: it fires an
interrupt at regular intervals no matter what code is running. The scheduler
hooks into that interrupt and can switch tasks on each tick.

## Programming the PIT

The 8253/8254 PIT chip has a base clock frequency of 1,193,182 Hz. You give it a
divisor and it counts down, fires IRQ0 when it hits zero, and reloads. For 100 Hz:
`1,193,182 / 100 ≈ 11,931`.

You write the configuration to I/O port `0x43` (command) and the divisor to port
`0x40` (low byte then high byte). The command byte `0x36` selects channel 0, square
wave mode, binary count.

After that the PIT is ticking. We install a handler via `irq_install_handler(0, pit_tick)`,
unmask IRQ0, and call `sti` to globally enable hardware interrupts on the CPU for the
first time. `pit_tick` just increments a counter so `pit_get_ticks()` returns the
current tick count.

## Proving it works

`kernel_main` reads the tick counter, waits in a `hlt` loop until 3 ticks have passed,
then prints `TIMER_OK`. The `hlt` instruction sleeps the CPU until the next interrupt
fires, so each wake-up is the timer. If `TIMER_OK` appears, IRQ0 is getting through
the PIC, the IDT entry is routing to our handler, and EOI is working (otherwise the
PIC would stop after one tick).
