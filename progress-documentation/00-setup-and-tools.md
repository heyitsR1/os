# 00 — Setup and tools

Before writing any kernel code we figured out what tools to use and why the normal
Mac compiler doesn't work for this.

## Why the normal compiler won't work

When you run `gcc hello.c` on macOS, the compiler assumes an OS exists underneath —
it links against macOS libraries, expects `printf` to work, expects memory to already
be managed. That's fine for regular programs, but our kernel *is* the OS. There's
nothing underneath it.

The fix is a **cross-compiler**: a version of GCC that runs on your Mac but produces
code for a completely different target. Ours is `i686-elf-gcc` — `i686` names the
target CPU (32-bit x86) and `elf` is the object file format. It compiles code that
runs on bare metal with no OS assumed.

## Tools

- **`i686-elf-gcc`** — the cross-compiler
- **`nasm`** — assembler for the hand-written assembly (boot stub, context switch)
- **`qemu-system-i386`** — emulates a complete 32-bit x86 PC in software so we don't
  have to reboot a real machine every time we test
- **`make`** — builds only what changed

## Two boot paths

We boot the kernel two ways depending on what we need:

1. **`-kernel` flag in QEMU** — loads our binary directly, no bootloader involved.
   Fast. Used for all day-to-day testing (`make run`, `make test`).
2. **GRUB ISO** — produces a proper bootable `.iso` the way a real PC would boot it.
   Takes longer to build. `make iso` produces it for the final demo.

Same kernel binary either way.

## How we test

Kernel code can't be unit-tested normally. Our approach: each subsystem prints a
short marker to the serial port (`BOOT_OK`, `KBD_OK`, etc.) when it comes up.
`scripts/test.sh` boots the kernel in QEMU, captures that serial output, and checks
the expected marker is there. So `PASS: found 'MP_OK'` means every subsystem up to
multiprocessing initialized cleanly.
