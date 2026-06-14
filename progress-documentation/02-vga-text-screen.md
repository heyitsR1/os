# 02 — Putting text on the actual screen (VGA)

**What we built:** the OS can now write text to the **QEMU window** itself, not just
the hidden serial debug wire. This is the display a person watching the demo sees.

## The neat trick: the screen is just memory

On a classic PC in text mode, the screen is **not** something you draw to with a
graphics library. Instead, there's a special region of memory starting at the fixed
address **`0xB8000`**, and whatever bytes you write there *instantly appear on
screen*. The hardware continuously reads that memory and paints it.

The screen is 80 columns × 25 rows = 2000 character cells. Each cell is **2 bytes**:

```
byte 0: the character to show (an ASCII code, e.g. 'A')
byte 1: the color  ->  low 4 bits = text color, high 4 bits = background color
```

So to put a white-on-blue 'A' in the top-left corner, you literally write the right
two bytes to address `0xB8000`. That's the entire "graphics driver." No library, no
API — just poking memory at a known address. This is one of those things that sounds
like it should be hard and turns out to be almost embarrassingly simple.

> The word **VGA** here just refers to this legacy text-mode memory layout. We are
> deliberately staying in text mode (no pixel graphics) — it's all the project needs.

## What `kernel/vga.c` actually does

It's a tiny terminal emulator built on that poke-the-memory idea:

- **`vga_init`** — fills all 2000 cells with blank spaces (clears the screen).
- **`vga_putc(c)`** — writes one character at the current cursor position, then moves
  the cursor right. The driver keeps track of the current row/column in two
  variables (the hardware doesn't track this for us in any way we use).
- **Wrapping** — when the cursor reaches column 80, it drops to the next line.
- **Scrolling** — when it runs off the bottom (past row 25), it copies every row up
  by one (row 1 becomes row 0, etc.) and blanks the bottom row, exactly like a real
  terminal scrolling up. This is just a memory copy.
- **`vga_write(s)`** — calls `vga_putc` for each character in a string.

One important detail: the memory pointer is marked **`volatile`**. That's a promise
to the compiler: "this memory has side effects you can't see — never optimize away
writes to it, even if it looks like the value is unused." Without that, an
optimizing compiler might decide our screen writes are pointless and delete them.

## Two outputs at once: `klog`

`kernel/kernel.c` now has a small helper called **`klog`** ("kernel log") that sends
each message to **both** the VGA screen and the serial wire:

```
klog("VGA_OK\n")  ->  shows up in the QEMU window  AND  on the serial debug log
```

This is convenient: a human watching the demo sees it on screen, and our automated
test (which only reads the serial wire) also sees it. Same message, two audiences.

The very first `BOOT_OK` still goes to serial only — it's printed *before* the VGA
screen is initialized, so there's no screen to write to yet.

## Where we are now

- ✅ On-screen text works: `make run` shows a white-on-blue `omen OS - phase 1` and
  `VGA_OK` in the QEMU window.
- ✅ `klog` mirrors output to screen + serial, so demos and tests both work.
- ✅ Automated test passes (`PASS: found 'VGA_OK'`).
- ⏭️ Next: the **GDT** — a required setup table that defines how the CPU views memory
  segments. It's a prerequisite before we can handle interrupts (the table after it,
  the IDT, refers back to it).

**Jargon recap:** *VGA text mode* = the legacy 80×25 screen that lives at memory
address `0xB8000`; writing bytes there shows characters. *cell* = one character +
its color, 2 bytes. *volatile* = a marker telling the compiler "don't optimize away
these memory accesses." *klog* = our helper that prints to screen and serial at once.
