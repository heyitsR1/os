# 06 — PIT timer: the OS heartbeat

**What we built:** a **PIT (Programmable Interval Timer) driver** that fires IRQ0
at 100 Hz, giving the kernel a steady heartbeat it can use to measure time and —
crucially — to preemptively switch between tasks later.

## The big idea: why an OS needs a timer

Without a timer, the kernel has no way to take back control from a running task.
If a task just loops forever, the CPU is stuck. The solution is a hardware timer
that interrupts the CPU at a fixed frequency, no matter what's running. Each tick
the interrupt fires, the kernel's handler runs, and the scheduler can decide whether
to keep running the current task or switch to another. This is the foundation of
**preemptive multitasking**.

The chip that does this on x86 is the **8253/8254 PIT** (Programmable Interval
Timer). It has a crystal that oscillates at exactly **1,193,182 Hz**. You give it a
**divisor** and it counts down from that value, fires IRQ0 when it hits zero, and
reloads — forever. To get 100 Hz you compute `1,193,182 / 100 ≈ 11,931` and
write that as the divisor.

## How we program the PIT

The PIT is configured by writing two things to I/O ports:

```
Port 0x43 (command): which channel, how to read/write the count, which mode
Port 0x40 (channel 0 data): the 16-bit divisor, low byte then high byte
```

Our command byte is `0x36`:

```
Bits 7-6: 00 = channel 0
Bits 5-4: 11 = access lo then hi byte
Bits 3-1: 011 = mode 3 (square wave generator — most stable for timekeeping)
Bit  0:   0 = binary count (not BCD)
```

Then we write the divisor in two bytes (low then high), and the PIT starts ticking.

```c
uint32_t divisor = 1193182 / hz;   // 100 Hz → 11931
outb(PIT_CMD, 0x36);
outb(PIT_CHANNEL0, divisor & 0xFF);
outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
```

## Hooking it into the IRQ system

Once the PIT is programmed, we need it to actually reach the CPU. Two things had to
happen first — and both were already done in the previous steps:

1. The IDT has an `irq0` gate at vector 0x20 (the PIC remap step ensured IRQ0
   lands there).
2. The `irq_handler` dispatch table and `irq_install_handler()` are in place.

So all we do here is register our tick handler and unmask IRQ0:

```c
irq_install_handler(0, pit_tick);   // "when IRQ0 fires, call pit_tick()"
pic_clear_mask(0);                  // allow IRQ0 through the PIC
```

And in `kernel_main`, before `pit_init()`, we call `sti` (Set Interrupt Flag) to
globally enable hardware interrupts on the CPU. This is the switch that turns the
whole interrupt system on for the first time.

## Proving it works

After `pit_init(100)`, the kernel reads `pit_get_ticks()`, spins in a `hlt` loop
until 3 ticks have passed, then prints `TIMER_OK`. The `hlt` instruction sleeps the
CPU until the next interrupt — each wake is the timer firing — so this is also an
implicit test that `hlt` + `sti` works correctly. Seeing `TIMER_OK` proves:

- The PIT is counting at the right frequency.
- IRQ0 is unmasked and getting through the PIC.
- The IDT entry at vector 0x20 is routing to our C handler.
- `pic_send_eoi()` is working (otherwise the PIC would stop after one tick).
- The global interrupt flag (`sti`) is set correctly.

Full serial output after this step:
```
BOOT_OK → VGA_OK → GDT_OK → IDT_OK → ISR3_OK → PIC_OK → PIT_OK → TIMER_OK
```

## Where we are now

- ✅ PIT programmed at 100 Hz; `ticks` counter increments every 10 ms.
- ✅ IRQ0 fully wired: PIT → PIC → IDT → `irq_handler` → `pit_tick` → EOI.
- ✅ `pit_get_ticks()` provides a monotonic tick counter for future use.
- ✅ `sti` enabled — hardware interrupts are globally live for the first time.
- ✅ Verified: kernel waits for 3 ticks, prints `TIMER_OK`, exits cleanly.
- ⏭️ Next: **keyboard driver (IRQ1)** — read PS/2 scancodes from port 0x60 and
  translate them to ASCII keystrokes. The IRQ system is already wired; we just need
  the handler and a scancode map.

**Jargon recap:** *PIT* = Programmable Interval Timer, the 8253/8254 chip that
generates periodic interrupts. *1,193,182 Hz* = the PIT's base clock frequency,
derived from the original IBM PC's 14.318 MHz crystal ÷ 12. *divisor* = the value
the PIT counts down from before firing; determines tick frequency. *sti* = "Set
Interrupt Flag" — the x86 instruction that globally enables hardware interrupts.
*hlt* = halt the CPU until the next interrupt (power-efficient idle). *preemptive
multitasking* = the scheduler can interrupt a running task at any tick, without the
task cooperating.
