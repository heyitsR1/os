# 07 — Keyboard driver

The keyboard driver reads PS/2 scancodes from IRQ1 and translates them to ASCII.

## How a keypress works at the hardware level

When you press a key, the PS/2 keyboard controller sends a **scancode** byte to the
CPU via IRQ1. Scancodes are not ASCII — they're raw hardware codes for which key
moved. A press sends a "make code" and a release sends a "break code" (same number
with the high bit set).

The driver's job each time IRQ1 fires:
1. Read the scancode from I/O port `0x60`
2. If the high bit is set, it's a key release — update modifier state and return
3. Track shift and caps-lock state
4. Look up the ASCII value in a translation table
5. Push the character into a ring buffer

## Translation tables

We use two 58-entry arrays: one for unshifted characters and one for shifted. Shift
and caps-lock interact the standard way — for letters they XOR (both on means
lowercase), for symbols only shift matters.

We're using PS/2 scancode set 1 (the BIOS default, which QEMU also uses). Every key
has a make code in the range `0x01`–`0x7F`.

## Ring buffer

Scancodes arrive asynchronously at interrupt time. We can't block the interrupt
handler waiting for the kernel to consume a character, so we use a 64-entry ring
buffer with separate head (write) and tail (read) indices. The handler writes to
head; `keyboard_getc()` reads from tail. If the buffer is full, the newest keystroke
is dropped.

## Testing

In the headless QEMU test environment there's no keyboard to press, so we can't
verify character decoding at boot time. `KBD_OK` just confirms the driver initialized
and IRQ1 is installed without crashing. In interactive use the driver is ready the
moment a key is pressed.
