# 01 — Booting & the first sign of life (`BOOT_OK`)

**What we built:** the smallest possible thing that counts as "our OS ran." When you
power on a (virtual) PC, control eventually lands in *our* code, which prints the
word `BOOT_OK`. That single word proves the entire pipeline — compile, link, boot,
run, report — works.

## The journey from power-on to our code

When a real PC turns on, a chain of hand-offs happens. We skip the first links by
using GRUB / QEMU, but it's worth seeing the whole chain:

```
Power on → firmware (BIOS) → bootloader (GRUB, or QEMU's built-in loader)
        → OUR boot stub (boot/multiboot.asm) → OUR kernel_main() in C
```

Each arrow is one program handing control to the next. Our job in this task was
only the **last two arrows**: write the tiny stub that the bootloader hands off to,
and have it call our C code.

## The Multiboot "handshake"

How does GRUB know our file is a kernel it can boot, and not just a random program?
There's an agreed-upon convention called **Multiboot**. The deal is:

- Our kernel must contain a special **magic number** (`0x1BADB002`) near the very
  start of the file. GRUB scans the first few kilobytes looking for it. Finding it
  means "yes, this is a Multiboot kernel, I know how to load it."
- In return, the bootloader promises to leave the CPU in a known, sane state:
  already in **32-bit protected mode**, with a basic memory setup, and it drops a
  different magic number (`0x2BADB002`) in a CPU register as proof that *it* really
  was a Multiboot-compliant loader.

This handshake is why we don't have to write the painful early boot code ourselves
(switching the CPU from ancient 16-bit "real mode" up to 32-bit mode, etc.). That's
the whole reason the project chose GRUB instead of a hand-rolled bootloader.

The file `boot/multiboot.asm` is written in **assembly** (not C) because it's the
very first thing to run — before C is usable. It does three small things:
1. Declares that magic number so GRUB recognizes us.
2. Sets up a **stack** (a scratch region of memory that C functions need to hold
   local variables and return addresses — C literally cannot run without one).
3. Calls our C function `kernel_main()` and, if it ever returns, halts the CPU.

> *Why assembly here?* C needs a stack and a few guarantees before it can run. The
> stub's job is to create those guarantees by hand, then jump into C. It's about 15
> lines and we never touch it again.

## The linker script: deciding *where* code lives in memory

`linker.ld` is a short recipe that tells the build tool: "place this kernel starting
at the **1 megabyte** mark in memory, and make sure that Multiboot magic number
comes first." Memory layout matters at this level because there's no OS to manage
memory for us yet — we are the OS. 1 MB is the conventional safe spot (the area
below it is cluttered with legacy hardware regions).

## Talking to the outside world: the serial port

We have no `printf`, no screen library, nothing. So how does `kernel_main` say
`BOOT_OK`? It uses the **serial port** — an ancient, dead-simple way for a computer
to send text out one character at a time over a wire. It's trivial to program (just
write bytes to a specific hardware address) and, crucially, QEMU can capture that
wire and dump it to our terminal. That makes it the perfect debug channel.

`kernel/serial.c` does exactly this: a short setup routine configures the serial
chip, then `serial_write("BOOT_OK\n")` pushes those characters out the wire one by
one. The matching `include/io.h` holds the two fundamental verbs for talking to
hardware on x86: `outb` (send a byte to a hardware "port") and `inb` (read a byte
from one). Almost every driver we write is ultimately built from these two.

## How we prove it works

`scripts/test.sh` boots the kernel inside QEMU with no window, captures the serial
output to a file, and checks whether `BOOT_OK` showed up. The `Makefile` wraps the
common commands so we just type:

- `make` — compile and link everything into `build/kernel.bin`
- `make run` — boot it in QEMU and watch the serial text live
- `make test EXPECT=BOOT_OK` — automated pass/fail check

> **Heads-up for later:** a bare `make test` (with no `EXPECT=`) checks for
> `TIMER_OK`, which doesn't exist until Task 6. So early on we always pass the
> codeword explicitly, e.g. `make test EXPECT=BOOT_OK`. A failure there just means
> "that codeword isn't printed yet," not that something broke.

## Where we are now

- ✅ A bootable kernel that GRUB/QEMU recognizes via the Multiboot handshake.
- ✅ It runs our C `kernel_main()` and emits `BOOT_OK` over the serial wire.
- ✅ Automated test confirms it (`PASS: found 'BOOT_OK'`).
- ⏭️ Next: give the OS an actual on-screen text display (the VGA terminal) so the
  QEMU *window* shows output too, not just the serial debug wire.

**Jargon recap:** *real mode / protected mode* = the CPU's old 16-bit startup
personality vs. the modern 32-bit one; GRUB hands us the machine already in
protected mode. *Multiboot* = the convention that lets GRUB recognize and load our
kernel. *stack* = scratch memory every C program needs. *linker script* = the recipe
that decides where pieces of our program sit in memory. *port I/O (`outb`/`inb`)* =
the basic way x86 software pokes hardware. *serial port* = a simple one-character-
at-a-time text wire we use as our debug console.
