# omen OS — BSIT 338 Final Project

Custom x86 kernel written from scratch for BSIT 338 (Operating Systems), Summer 2026.
Team: Aarohan Niraula, Bishesh Raut, Karan Tamang.

## What it does

Boots on a virtual x86 PC and brings up the core pieces of an operating system:

- GRUB bootloader + our own multiboot stub
- GDT and IDT (the CPU-level tables every protected-mode OS needs)
- PIC remapping so hardware interrupts don't collide with CPU exceptions
- PIT timer running at 100 Hz
- PS/2 keyboard and mouse drivers
- Physical memory manager and paging
- Kernel heap (`kmalloc`)
- Kernel threads with hand-written context switching
- Preemptive round-robin CPU scheduler
- Multiple processes with isolated address spaces (CR3 swap)
- An interactive keyboard-driven shell that demos each feature on demand

Everything runs in ring 0, 32-bit protected mode. No user mode, no filesystem, no SMP.

## The shell

`make run` drops you at an `omen>` prompt. Type `help` for the command list:

- `about`, `clear`, `echo`, `meminfo`, `uptime` — informational commands
- `threads` — spawn two interleaved kernel threads (context switch demo)
- `sched`   — two non-yielding workers under preemption (scheduler demo)
- `proc`    — two processes sharing a virtual address, isolated (process demo)
- `mouse`   — live mouse cursor tracking; press any key to exit
- `reboot`  — quit QEMU

The shell reuses the same kernel primitives the boot self-tests exercise, so each
command is a live, on-demand version of the corresponding feature.

## Building and running

You need the `i686-elf` cross-compiler toolchain and QEMU installed.

```bash
make            # compile everything into build/kernel.bin
make test       # run headless in QEMU and check all markers pass
make run        # run with serial output printed to your terminal
```

To test a specific phase:
```bash
make test EXPECT=THREADS_OK
make test EXPECT=SCHED_OK
make test EXPECT=MP_OK
```

## Project structure

```
boot/        multiboot stub (assembly)
kernel/      kernel_main, GDT, IDT, ISRs, PIC, PIT, keyboard, mouse, serial, VGA
mm/          physical memory manager, paging, kmalloc
sched/       task struct, context_switch (assembly), scheduler, process isolation
shell/       interactive REPL — line editing, command table, feature demos
scripts/     test.sh — headless QEMU test harness
linker.ld
Makefile
```

## What the output means

When you run `make test` you'll see lines like:
```
BOOT_OK → VGA_OK → GDT_OK → IDT_OK → ISR3_OK → PIC_OK → PIT_OK → TIMER_OK →
KBD_OK → MOUSE_OK → PMM_OK → PAGING_OK → HEAP_OK → KMALLOC_OK →
BABABABABA → THREADS_OK → SCHED_OK → MP_OK
```

Each marker means that subsystem initialized and passed its test. `BABABABABA` is two
threads alternating — proof the context switch is actually working. `SCHED_OK` means two
CPU-hog threads both made progress without ever yielding, so the preemptive timer is
doing its job. `MP_OK` means two processes wrote to the same virtual address and each
read back their own value — isolated address spaces confirmed.

## Progress docs

`progress-documentation/` has a write-up for each feature explaining what we built
and how it works.
