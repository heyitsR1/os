# 02 — VGA text display

Now the OS can write text to the QEMU window, not just the serial debug wire.

## How VGA text mode works

On a classic PC in text mode, the screen is a region of memory starting at
`0xB8000`. Whatever you write there immediately appears on screen — the hardware
continuously reads that memory and renders it. No library needed, no API. You just
write bytes.

The screen is 80 columns × 25 rows. Each character cell is 2 bytes:
- byte 0: the ASCII character
- byte 1: color (low 4 bits = text color, high 4 bits = background color)

So putting a white-on-blue 'A' in the top-left corner means writing two specific
bytes to `0xB8000`. That's it. It sounds like it should be harder.

## What `vga.c` does

It's a small terminal built on top of that memory trick:

- **`vga_init`** — fills all 2000 cells with spaces (clears the screen)
- **`vga_putc(c)`** — writes one character at the current cursor position and
  advances the cursor
- **Wrapping** — cursor hits column 80, drops to the next line
- **Scrolling** — cursor runs past row 25, every row copies up by one and the
  bottom row is blanked (just a `memmove` on that memory region)
- **`vga_write(s)`** — calls `vga_putc` for each character in a string

One thing worth noting: the VGA memory pointer has to be marked `volatile`. Without
it, an optimizing compiler might decide writes to it are pointless (nobody reads the
value back in C) and remove them. `volatile` tells the compiler "don't touch these."

## Dual output

`kernel_main` has a small `klog()` helper that sends every message to both the VGA
screen and the serial wire. A person watching the demo sees it on screen; the test
script sees it on serial. The very first `BOOT_OK` still goes to serial only because
it's printed before the VGA screen is set up.
