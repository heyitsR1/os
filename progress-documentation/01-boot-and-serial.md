# 01 — Boot and serial output

Getting the kernel to boot and print `BOOT_OK` was the first milestone — it proves
the whole compile/link/boot pipeline works before we write any real OS features.

## How boot works

We're using GRUB as the bootloader so we don't have to write the painful early
startup code (switching the CPU from 16-bit real mode to 32-bit protected mode, etc.).
GRUB handles all that and hands us a CPU already in protected mode.

The deal is: our kernel file has to contain a specific magic number (`0x1BADB002`)
near the start. GRUB scans the file looking for it. If it finds it, it knows this is
a Multiboot-compliant kernel and loads it. In return GRUB leaves a different magic
(`0x2BADB002`) in a register so we can verify it actually ran.

`boot/multiboot.asm` is the first code that executes. It:
1. Declares the Multiboot magic number so GRUB finds us
2. Sets up a stack (C literally can't run without one — it needs somewhere to put
   local variables and return addresses)
3. Calls `kernel_main()` in C

It's about 15 lines of assembly and we don't touch it again.

## Linker script

`linker.ld` tells the linker where to place things in memory. We load the kernel at
the 1 MiB mark (`0x100000`) because that's the standard safe start address on x86 —
everything below it has hardware-reserved regions. The script also makes sure the
Multiboot header lands early in the file so GRUB can scan for it.

## Serial output

There's no `printf` in a kernel. To print anything we use the **serial port** — a
simple hardware interface where you write bytes to an I/O port and they come out the
other end. QEMU captures that wire and dumps it to the terminal.

`kernel/serial.c` sets up the serial chip and gives us `serial_write()`. The two
primitive functions in `include/io.h` are `outb` (write a byte to a hardware port)
and `inb` (read one back). Nearly everything hardware-facing in this kernel uses them.

## Testing

`make test EXPECT=BOOT_OK` boots the kernel in QEMU headlessly, captures serial
output, and checks that `BOOT_OK` appears. It passing means compilation, linking, and
the bootloader handoff all worked.
