# Custom OS — BSIT 338 Final Project

A small x86 protected-mode operating system kernel built from scratch for the
BSIT 338 Operating Systems course (Summer 2026), by team **omen**
(Aarohan Niraula, Bishesh Raut, Karan Tamang).

## ➢ What this is

A bare-metal kernel that boots via GRUB, sets up its own segment and interrupt
tables, and implements the core subsystems every general-purpose OS needs:

- Boot (GRUB2 + Multiboot1)
- Global Descriptor Table (flat ring-0 segments)
- Interrupt Descriptor Table, exception handlers, and PIC remapping
- A programmable interval timer driving a periodic tick
- PS/2 keyboard driver (scancode → ASCII input)
- PS/2 mouse driver (position and button state)
- Physical memory management and paging
- A kernel heap allocator (`kmalloc`)
- Kernel-level multithreading with hand-written context switching
- A preemptive, round-robin CPU scheduler
- Multiprocessing via multiple task contexts, each with its own page directory

## ➢ What we will do

- Keep everything running in 32-bit protected mode (i686), in ring 0
- Use a small assembly stub to satisfy the Multiboot header and hand off to a
  C `kernel_main()`
- Build and test with a prebuilt `i686-elf` cross-compiler and `qemu-system-i386`
- Implement "processes" as pre-compiled C functions running as separate task
  contexts, each with its own CR3 (page directory)
- Keep the build simple: a plain Makefile, no extra build tooling

## ➢ What we won't do

- Write a custom bootloader, filesystem driver, or build a cross-compiler from
  source — we rely on off-the-shelf, well-tested equivalents (GRUB, prebuilt
  `i686-elf-gcc`)
- Implement a filesystem of any kind
- Support user-mode programs, an ELF loader, or a syscall interface
- Support USB devices — only PS/2 keyboard and mouse (fully supported by QEMU)
- Go beyond basic VGA text mode for graphics
- Implement ACPI or multi-core (SMP) support — this is a single-CPU,
  software-multitasked design

## ➢ Project structure

```
boot/        # Multiboot stub (assembly)
kernel/      # kernel_main, GDT, IDT, ISRs, PIC, PIT
drivers/     # keyboard.c, mouse.c
mm/          # physical memory manager, paging, kmalloc
sched/       # task struct, context_switch (assembly), scheduler
Makefile
linker.ld
```

## ➢ Building and running

Requires the `i686-elf` cross-compiler toolchain and QEMU.

```bash
make
qemu-system-i386 -kernel build/kernel.bin
```
