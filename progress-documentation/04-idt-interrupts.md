# 04 — IDT and interrupt handling

The IDT (Interrupt Descriptor Table) is the table that tells the CPU what to do when
an interrupt fires — which function to call for each of the 256 possible interrupt
numbers. This is the plumbing that everything hardware-facing depends on: keyboard,
mouse, the timer.

## Types of interrupts

Three sources:
- **Exceptions (vectors 0–31)** — the CPU raises these on errors: divide-by-zero,
  accessing unmapped memory, illegal instructions, etc.
- **Hardware interrupts (IRQs)** — devices raise these: timer tick, key pressed,
  mouse moved. We hook into these in the next few steps.
- **Software interrupts** — code triggers one with the `int` instruction. We use this
  to test the handler.

## How it's structured

We can't write 256 separate C functions, so the approach is:
- 32 tiny assembly stubs, one per exception vector (generated with a macro since
  they're nearly identical)
- All stubs funnel into a single C function `isr_handler()`
- 16 more stubs for hardware IRQs (vectors 32–47) funneling into `irq_handler()`

Each stub saves all the CPU registers onto the stack, calls the C handler with a
pointer to those saved registers, then restores everything and runs `iret` (interrupt
return) to resume whatever was interrupted.

One detail that had to be exactly right: some CPU exceptions push an extra "error
code" onto the stack automatically, and others don't. To keep the saved-register
layout consistent, the stubs for the ones that don't push a dummy zero. The list of
which exceptions push an error code (vectors 8, 10–14, 17, 21, 30) has to be precise
— one wrong entry and the layout is off by 4 bytes, which corrupts register restores.

## Testing it

We fire a deliberate `int $0x3` (the breakpoint exception) right after setting up the
IDT. The handler checks for vector 3, prints `ISR3_OK`, and returns normally. The
kernel then continues running. That round trip — CPU pauses, finds our handler, runs
our C code, restores state, resumes — is proof the whole mechanism works. Any real
exception (not vector 3) would print the exception name and halt.

## Hardware IRQ setup

The IRQ stubs are in place here, but they're not active yet — that needs the PIC
remapped in the next step. We also added a registry where drivers can plug in their
own handler per IRQ (`irq_install_handler(irq, fn)`), which keyboard, mouse, and
the timer will use.
