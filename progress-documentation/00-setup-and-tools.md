# 00 — Setup & Tools (what all these programs actually do)

> This series explains *what we're building and why*, in plain language. You know
> OS concepts; these docs decode the **tools and jargon** so you can follow along
> without writing the code yourself. Each doc maps to one task in the build plan.

## The one-sentence version

To build an OS you need a way to **turn our C code into a program the bare machine
can run** (no Windows/macOS underneath it), and a way to **run that program on a
fake computer** so we don't have to reboot a real PC every time. That's the whole
toolchain.

---

## The problem normal compilers have

When you run `gcc hello.c` on your Mac, the compiler quietly assumes a *lot*:
that there's an operating system, that `printf` exists, that there's a screen and
files and memory already set up for you. It builds a program that asks macOS to do
all the hard parts.

Our kernel **is** the operating system. There's nothing underneath it. So we can't
use the normal Mac compiler — it would bake in macOS assumptions and produce a
program that only runs *inside* macOS. We need a compiler that assumes **nothing**.

## The tools we installed, and the job each one does

| Tool | Plain-language job |
|------|--------------------|
| **`i686-elf-gcc`** | A special "cross-compiler." *Cross* = it runs on your Mac (Apple Silicon) but produces code for a **different** kind of CPU: a 32-bit Intel x86 chip. *i686* names that target CPU. It assumes no operating system exists — exactly what we need. |
| **`i686-elf-binutils`** | The cross-compiler's helpers: the **assembler** (turns human-written CPU instructions into raw bytes) and the **linker** (stitches all our compiled pieces into one final program file). |
| **`nasm`** | A second assembler we use for the hand-written assembly bits (the very first boot code and a few low-level routines). We use NASM because its syntax is the standard in OS tutorials and is easy to read. |
| **`qemu` (`qemu-system-i386`)** | A **virtual computer**. It pretends to be a full 32-bit Intel PC — CPU, RAM, screen, keyboard, timer chip — entirely in software. We boot our OS inside it. If our kernel crashes, only a window closes; nothing on your real machine is harmed. This is our test bench. |
| **`i686-elf-grub`** | **GRUB** is a real, famous **bootloader** — the program that runs first when a PC powers on and then loads an operating system. We use it instead of writing our own. This package is the version that targets our 32-bit CPU. We use it to make a bootable CD image at the end. |
| **`xorriso`** | A utility that packs files into a `.iso` (a CD/DVD disc image). GRUB uses it under the hood to produce a bootable disc image of our OS. |
| **`make`** | The **build manager**. Instead of typing five compiler commands in the right order every time, we write the recipe once in a file called `Makefile` and just run `make`. It also knows to only rebuild what changed. |

## Two ways we'll boot — and why we have both

There are two paths from "our code" to "running OS," and we deliberately use both:

1. **The fast path (daily work):** QEMU can load our kernel *directly* with a flag
   called `-kernel`, skipping GRUB entirely. This makes the build-test loop fast,
   which matters when we're iterating dozens of times. This is what `make run` and
   `make test` use.

2. **The "real" path (the deliverable):** At the end we also produce a proper
   bootable `.iso` that **GRUB** loads, exactly like a real PC boots from a disc.
   This is the authentic bootloader artifact to show for the project. It's slower
   to build, so we only do it on demand with `make iso`.

Both paths run the *same* kernel; only the thing that loads it differs.

## How we'll know it works (without reading code)

Real OS code can't be checked with ordinary "unit tests." So our trick is: **every
part of the kernel prints a short codeword** to a serial port (think of it as a
debug text wire QEMU can capture). For example, finishing boot prints `BOOT_OK`;
the timer working prints `TIMER_OK`.

A small script boots the kernel inside QEMU, reads those codewords, and reports
PASS/FAIL by checking the expected codeword showed up. So when you see
`PASS: found 'TIMER_OK'`, it literally means "the OS booted far enough to get its
timer interrupt firing." Each task in this project adds one more codeword.

## Where we are now

- ✅ All tools installed and verified (`i686-elf-gcc -dumpmachine` reports `i686-elf`).
- ✅ Told git to ignore the `build/` output folder so we never commit compiled junk.
- ⏭️ Next: write the tiny first program that a bootloader can hand control to, and
  prove the whole pipeline (compile → link → boot in QEMU → print `BOOT_OK`) works.

**Jargon recap:** *cross-compiler* = builds code for a different CPU than the one
you're on. *bootloader* = the first program that runs at power-on and loads the OS.
*QEMU* = a fake PC in software. *serial port* = a simple text output wire we use to
spy on what the kernel is doing.
