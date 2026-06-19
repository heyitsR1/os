# 05 — PIC remap

The 8259 PIC (Programmable Interrupt Controller) is the chip that sits between
hardware devices and the CPU. Devices signal the PIC when they need attention
(keyboard pressed, timer ticked, etc.), and the PIC forwards one at a time to the
CPU as an interrupt.

## The problem

The 8259 was designed for 16-bit real mode. By default it maps IRQ0 to CPU vector 8,
IRQ1 to vector 9, etc. But in 32-bit protected mode, vectors 0–31 are reserved for
CPU exceptions (divide-by-zero, page fault, etc.). If we left the PIC at its default,
a timer tick would come in as vector 8, which the CPU would interpret as a Double Fault
exception — and we'd crash.

The fix is to reprogram the PIC so it delivers IRQs at higher vector numbers that
don't conflict. We tell it: master PIC's IRQ0 → vector 0x20, IRQ1 → 0x21, etc.,
and slave PIC's IRQ8 → 0x28, and so on. Those line up with the IRQ stubs we put in
the IDT in the previous step.

## How it's done

The 8259 is configured by writing a specific sequence of bytes to its I/O ports.
There are two PICs (master and slave — the slave's output feeds into IRQ2 of the
master). Both need to be reprogrammed, and there's an exact sequence of writes that
has to happen in order: send initialization command, send new base vector, configure
the cascade connection, set operating mode. The `io_wait()` calls between writes give
the chip time to process each byte — it's slow compared to the CPU.

After the remap we save and restore the mask registers so any IRQs that were already
masked stay masked.

## EOI

Every time the kernel handles an IRQ it has to send an End-Of-Interrupt signal back
to the PIC, otherwise the PIC won't forward any more interrupts. `pic_send_eoi(irq)`
does this — it sends `0x20` to the master (and also to the slave if the IRQ came from
one of the slave's lines). The IRQ handler calls it after every dispatch.

## Masks

Each PIC has an 8-bit mask register. A `1` bit means that IRQ is suppressed. After
boot all IRQs are masked. Each driver enables its own IRQ by calling
`pic_clear_mask(irq)` when it initializes.
