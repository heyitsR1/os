# 07 — Keyboard driver: IRQ1 and PS/2 scancodes

**What we built:** a **PS/2 keyboard driver** on IRQ1 that translates raw
hardware scancodes into ASCII characters, tracks shift/caps-lock state, and
queues keystrokes in a ring buffer for the rest of the kernel to consume.

## The big idea: how a keypress reaches software

When you press a key, the PS/2 keyboard controller sends a **scancode** byte to
the CPU via IRQ1. The scancode is not an ASCII value — it is a raw number that
encodes which physical key moved and whether it was pressed (make code) or
released (break code). The driver's job is to:

1. **Catch the interrupt** — our IRQ1 handler runs.
2. **Read the scancode** from I/O port `0x60`.
3. **Filter break codes** — the high bit (bit 7) is set on key release; we
   ignore releases for regular keys but track them for modifiers.
4. **Track modifiers** — shift (left `0x2A` / right `0x36`) and caps-lock
   (`0x3A`) change how we translate the next key.
5. **Look up the character** in a translation table and push it to a ring buffer.

## Scancode sets and our choice

PS/2 keyboards can use three scancode sets. We use **set 1** (the BIOS default),
which QEMU also defaults to. In set 1, every key has a distinct make code in the
range `0x01`–`0x7F`; the corresponding break code is the make code OR'd with
`0x80`.

## The translation tables

We keep two 58-entry arrays: `sc_lower` (unshifted) and `sc_upper` (shifted).
Entry `[i]` holds the ASCII character for scancode `i`, or `0` for keys with no
printable representation (Ctrl, non-printable function keys, etc.).

Shift and caps-lock compose correctly:

| caps-lock | shift | letter result |
|-----------|-------|---------------|
| off       | off   | lowercase     |
| off       | on    | uppercase     |
| on        | off   | uppercase     |
| on        | on    | lowercase (XOR) |

Non-letter keys (digits, symbols) are only affected by the shift key, not
caps-lock, which matches standard keyboard behaviour.

## The ring buffer

Scancodes arrive asynchronously at interrupt time. We cannot block the interrupt
handler waiting for the kernel to consume a character, so we use a **ring buffer**:
a 64-byte circular array with separate `head` (write) and `tail` (read) indices.
The handler writes to `head`; `keyboard_getc()` reads from `tail`. If the buffer
is full the newest keystroke is silently dropped — acceptable for a kernel.

## Key files

| File | Role |
|------|------|
| `kernel/keyboard.c` | IRQ1 handler, modifier state, translation tables, ring buffer |
| `kernel/keyboard.h` | `keyboard_init()`, `keyboard_getc()` |

## Serial output

```
KBD_OK
```

Printed by `kernel_main` after `keyboard_init()` returns without fault. Because
the test harness runs headless (no real keyboard input), we cannot test actual
character decoding at boot — but the IRQ1 handler is installed and the driver is
ready to receive keystrokes the moment a key is pressed in interactive use.
