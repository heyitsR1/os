# 05 — PIC remap: teaching the hardware where to deliver interrupts

**What we built:** the **PIC remap** — a one-time configuration sequence that tells
the two interrupt controller chips in the PC to deliver hardware interrupts (IRQs)
to the correct IDT slots. Without this, the CPU would confuse device interrupts with
CPU exceptions and crash.

## The big idea: the 8259 PIC

When a hardware device wants attention — the keyboard says "a key was pressed," the
timer says "another millisecond passed" — it can't just yell at the CPU directly.
Instead it signals a chip called the **8259 Programmable Interrupt Controller (PIC)**.
The PIC's job is to sit between the devices and the CPU, buffer their requests, and
forward one at a time by raising the CPU's `INTR` line.

PCs have **two PICs** chained together: a *master* (PIC1) and a *slave* (PIC2).
The master handles IRQs 0–7; the slave handles IRQs 8–15, but feeds its output into
IRQ2 of the master so there's effectively one chain. Together they manage 15
hardware interrupt lines (IRQ2 is consumed by the cascade).

```
Devices            PIC1 (master)     PIC2 (slave)
─────────────────  ──────────────    ──────────────────
PIT timer → IRQ0   IRQ0  ─┐          IRQ8  (RTC)
Keyboard  → IRQ1   IRQ1  ─┤ → CPU    IRQ9
                   IRQ2  ── cascade ← IRQ10
                   IRQ3  ─┤          IRQ11
                   ...    ┘          IRQ12 (PS/2 mouse)
                                     IRQ14 (ATA primary)
                                     IRQ15 (ATA secondary)
```

## The problem: conflicting interrupt vectors

The 8259 was designed for 8086 real mode, where **IRQ0 maps to CPU vector 0x08**.
But we're in 32-bit protected mode, and **vectors 0x00–0x1F are reserved for CPU
exceptions** (divide-by-zero, page fault, etc.). If the PIC tries to deliver a timer
tick as vector 8, the CPU thinks it's a Double Fault exception and panics.

The fix is a **remap**: reconfigure the PIC to deliver IRQ0–7 at vectors 0x20–0x27
and IRQ8–15 at vectors 0x28–0x2F. Those slots are above all the CPU exceptions and
line up exactly with the `irq0`–`irq15` stubs we installed in the IDT in the
previous step.

## How the remap works: Initialization Command Words

The 8259 is configured by writing a precise sequence of bytes called **ICWs
(Initialization Command Words)** to its I/O ports. Sending ICW1 starts the sequence;
the PIC then expects ICW2, ICW3, ICW4 in order.

```c
// ICW1 (to command port): "start init sequence, ICW4 will follow"
outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);   // 0x10 | 0x01 = 0x11
outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

// ICW2 (to data port): "your interrupt vector base is..."
outb(PIC1_DATA, 0x20);    // master: IRQ0 → vector 0x20
outb(PIC2_DATA, 0x28);    // slave:  IRQ8 → vector 0x28

// ICW3: tell the chips about the cascade connection
outb(PIC1_DATA, 0x04);    // master: "slave is connected to my IRQ2" (bit 2)
outb(PIC2_DATA, 0x02);    // slave:  "I am the slave on cascade line 2"

// ICW4: "we're on an x86 (8086 mode), handle EOI normally"
outb(PIC1_DATA, ICW4_8086);   // 0x01
outb(PIC2_DATA, ICW4_8086);
```

The `io_wait()` calls between writes give old hardware time to process each byte —
the PIC is slow compared to modern CPUs and can miss writes that arrive too fast.

We save the masks before the sequence and restore them after — this preserves
whatever IRQs were already masked (disabled), since the remap itself doesn't change
which devices are allowed to interrupt.

## Masks and EOI

The code also provides two utility functions that future driver steps will use:

**Masks** control which IRQs the PIC forwards to the CPU. Each PIC has an 8-bit mask
register; a `1` bit means "suppress that IRQ." By default after boot all IRQs are
masked. A driver enables its IRQ by calling `pic_clear_mask(irq)` (clears the bit,
allowing it through). `pic_set_mask(irq)` re-disables it.

**EOI (End Of Interrupt)** is a signal the CPU must send back to the PIC after
handling an IRQ, telling it "I'm done, you can forward more interrupts now." Without
EOI the PIC goes silent — you'd get exactly one interrupt of each kind and then
nothing. Our `pic_send_eoi(irq)` sends `0x20` to the master PIC's command port (and
also to the slave if the IRQ was >= 8), and `irq_handler()` calls it at the end of
every IRQ handler.

## Where we are now

- ✅ Master PIC remapped: IRQ0–7 now map to IDT vectors 0x20–0x27.
- ✅ Slave PIC remapped: IRQ8–15 now map to IDT vectors 0x28–0x2F.
- ✅ Cascade wiring configured (IRQ2 → slave connection declared to both chips).
- ✅ `pic_send_eoi()`, `pic_set_mask()`, `pic_clear_mask()` in place for drivers.
- ✅ `irq_install()` has populated the IDT gates for all 16 IRQ stubs.
- ✅ Kernel prints `PIC_OK` and exits cleanly — no spurious exception fires.
- ⏭️ Next: **PIT timer (IRQ0)** — program the timer chip to fire at ~100 Hz and
  install a tick counter; this is the heartbeat that will later drive preemptive
  scheduling.

**Jargon recap:** *PIC* = Programmable Interrupt Controller, the chip that routes
device interrupts to the CPU. *IRQ* = Interrupt ReQuest line (the wire from a
device to the PIC). *vector* = the interrupt number the CPU looks up in the IDT.
*remap* = reconfiguring the PIC's base vector so IRQs land above CPU-exception
vectors. *ICW* = Initialization Command Word, the byte sequence that configures the
PIC. *EOI* = End Of Interrupt, the "I'm done" acknowledgement sent back to the PIC.
*mask* = a per-IRQ bit that suppresses delivery of that interrupt.
