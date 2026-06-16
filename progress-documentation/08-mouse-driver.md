# 08 — Mouse driver: IRQ12 and PS/2 aux packets

**What we built:** a **PS/2 mouse driver** on IRQ12 that initialises the 8042
auxiliary device, accumulates 3-byte movement packets, and exposes a
`mouse_state_t` struct with clamped (x, y) coordinates and button bits.

## The big idea: the 8042 as a dual-channel controller

The same Intel 8042 chip that handles the keyboard also hosts a second,
"auxiliary" PS/2 channel for the mouse. The two channels share:

- **Data port `0x60`** — reads deliver keyboard data *or* mouse data depending
  on which device just fired.
- **Status/command port `0x64`** — commands here control both channels.

To distinguish them at the IRQ level: keyboard fires **IRQ1**, mouse fires
**IRQ12** (via the slave PIC cascade on IRQ2).

## Initialisation sequence

```
1. Write 0xA8 to 0x64   → enable aux device
2. Write 0x20 to 0x64   → request "Compaq status byte"
   Read byte from 0x60  → get current controller config
   Set bit 1 (IRQ12 enable), clear bit 5 (mouse clock disable)
3. Write 0x60 to 0x64   → write modified status byte back
   Write modified byte to 0x60
4. Send 0xF6 to mouse   → "set defaults"   (expect ACK 0xFA)
5. Send 0xF4 to mouse   → "enable data reporting" (expect ACK 0xFA)
6. Install IRQ12 handler; unmask IRQ2 and IRQ12 in the PIC
```

All 8042 port accesses poll the status register's busy bits before reading or
writing, with a **timeout counter** so a non-responsive controller (e.g. the
headless QEMU test environment) doesn't hang the kernel.

## The 3-byte packet format

Every mouse movement or click produces a 3-byte packet:

| Byte | Contents |
|------|----------|
| 0    | Flags: Y-overflow, X-overflow, Y-sign, X-sign, always-1, mid, right, left |
| 1    | X delta (unsigned 8-bit; sign is bit 4 of byte 0) |
| 2    | Y delta (unsigned 8-bit; sign is bit 5 of byte 0) |

The handler accumulates bytes into a 3-element array and only processes the
packet once all three have arrived. Overflow packets (bytes 0 bits 6–7 set) are
discarded — the delta values are unreliable in that case.

## Sign extension and coordinate system

The PS/2 deltas are **9-bit two's-complement** values spread across the byte and
its sign bit in byte 0. We reconstruct the full signed value:

```c
int32_t dx = (int32_t)packet[1] - ((flags & 0x10) ? 256 : 0);
int32_t dy = (int32_t)packet[2] - ((flags & 0x20) ? 256 : 0);
```

Mouse Y is inverted relative to screen Y (mouse moves up → positive dy → row
decreases), so we subtract: `state.y -= dy`.

Coordinates are **clamped** to the VGA 80×25 text-mode screen bounds so
`mouse_get_state()` always returns a valid (col, row) pair.

## Key files

| File | Role |
|------|------|
| `kernel/mouse.c` | 8042 init, 3-byte packet FSM, IRQ12 handler, clamped state |
| `kernel/mouse.h` | `mouse_state_t`, `mouse_init()`, `mouse_get_state()`, screen constants |

## Serial output

```
MOUSE_OK
```

Printed by `kernel_main` after `mouse_init()` returns. The init may return
early if the 8042 times out (expected in `-display none` QEMU), but the IRQ12
handler is still registered and will activate when a real mouse is present.
