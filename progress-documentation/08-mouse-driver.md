# 08 — Mouse driver

The mouse driver initializes the PS/2 auxiliary device and reads 3-byte movement
packets from IRQ12.

## The 8042 controller

The same chip that handles the keyboard (the Intel 8042) also has a second "auxiliary"
channel for the mouse. Both share data port `0x60` and command/status port `0x64`.
The kernel tells them apart by which IRQ fires: keyboard is IRQ1, mouse is IRQ12 (via
the slave PIC cascade).

## Initialization

The init sequence has to go through the 8042 controller to enable the aux device:
1. Send `0xA8` to `0x64` — enable aux device
2. Read the controller config byte and set the bit that enables IRQ12
3. Write the config byte back
4. Send `0xF6` (set defaults) to the mouse and wait for ACK
5. Send `0xF4` (enable data reporting) and wait for ACK
6. Install the IRQ12 handler and unmask IRQ2 + IRQ12 in the PIC

All port accesses poll the controller status register before reading or writing —
the 8042 is slow. We also added timeout counters so a non-responsive controller
(like headless QEMU where there's no actual mouse) doesn't hang the kernel.

## Packet format

Each movement or click produces a 3-byte packet:
- Byte 0: flag bits (overflow flags, sign bits for X and Y, button states)
- Byte 1: X delta (unsigned, sign comes from byte 0 bit 4)
- Byte 2: Y delta (unsigned, sign comes from byte 0 bit 5)

The deltas are 9-bit two's-complement values split across the byte and a sign bit
in byte 0. We reconstruct the full signed delta and apply it to the current X/Y
position. Mouse Y is inverted relative to screen Y (moving up increases dy, but
decreases the screen row), so we subtract. Coordinates are clamped to 0–79 (columns)
and 0–24 (rows) so `mouse_get_state()` always returns a valid screen position.

## Testing

`MOUSE_OK` confirms the init sequence ran without hanging. In headless QEMU the
8042 aux init may time out partway through, but the IRQ12 handler is still registered
and will work with a real mouse in interactive use.
