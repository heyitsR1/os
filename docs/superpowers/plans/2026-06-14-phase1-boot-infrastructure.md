# Phase 1: Boot Infrastructure — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Boot an i686 protected-mode kernel via Multiboot1, print to VGA + serial, install a GDT, install an IDT with CPU-exception (ISR) and hardware-interrupt (IRQ) handling via a remapped PIC, and drive a periodic PIT timer tick.

**Architecture:** A NASM Multiboot1 stub sets up a stack and calls C `kernel_main()`. C code (compiled with the prebuilt `i686-elf-gcc` cross-compiler, linked at 1 MiB by `linker.ld`) brings up subsystems in dependency order: serial → VGA → GDT → IDT/ISR/PIC → PIT. Everything runs in ring 0 with flat segments. Verification is automated: each subsystem writes a unique marker string to the COM1 serial port, and a smoke-test script boots the kernel headless in QEMU, captures serial output, and greps for the expected marker.

**Tech Stack:** `i686-elf-gcc` / `i686-elf-binutils` (cross-compiler, via Homebrew), `nasm` (assembly), GNU `make`, `qemu-system-i386` (test/run), `i686-elf-grub` + `xorriso` (bootable ISO deliverable). Host: macOS arm64 (cross-compilation; QEMU runs i386 under TCG emulation).

**Covers roadmap items 1–4:** Boot, GDT, IDT/ISR/PIC, PIT timer.

**Git identity for all Phase 1 commits:** `Aarohan Niraula <aarohan567@gmail.com>` (the current repo identity — no per-commit override needed for Phase 1). Branch: `bootloader`.

---

## Conventions used throughout this plan

- **Working directory:** `/Users/aarohan/os/project` (the git repo). All paths below are relative to it.
- **Cross-compiler binaries** live in `/opt/homebrew/bin` (on `PATH` after `brew install`): `i686-elf-gcc`, `i686-elf-ld`, etc.
- **Build output** goes to `build/` (git-ignored). Source is never committed pre-built.
- **Serial markers:** every subsystem prints a line like `BOOT_OK\n` to COM1. Tests grep for these. Markers are uppercase, end with `\n`.
- **Smoke test command:** `make test EXPECT=MARKER` boots the kernel in QEMU and passes/fails on whether `MARKER` appears on serial. A bare `make test` checks for the final marker `TIMER_OK`.
- **Commits:** small and frequent, one per task (or per logical sub-step). Commit messages use Conventional Commits (`feat:`, `build:`, `docs:`, `chore:`).

---

## File Structure

Files created in this phase and their single responsibility:

| File | Responsibility |
|------|----------------|
| `boot/multiboot.asm` | Multiboot1 header + `_start`; set up stack, push multiboot args, call `kernel_main`, then halt. |
| `linker.ld` | Link the kernel ELF at 1 MiB; place `.multiboot` first so the header is within the first 8 KiB. |
| `Makefile` | Build `build/kernel.bin`, run QEMU (`run`), smoke-test (`test`), build ISO (`iso`), `clean`. |
| `include/types.h` | Freestanding fixed-width type convenience header (wraps `<stdint.h>`/`<stddef.h>`/`<stdbool.h>`). |
| `include/io.h` | Inline `inb`/`outb`/`outw`/`inw`/`io_wait` port I/O helpers. |
| `kernel/serial.h` / `kernel/serial.c` | COM1 (0x3F8) init + byte/string output, used for test markers and debug. |
| `kernel/vga.h` / `kernel/vga.c` | VGA text-mode 80×25 terminal at `0xB8000`: clear, putchar, write, scroll. |
| `kernel/kernel.c` | `kernel_main()`: orchestrates subsystem init in order, prints markers, runs the timer demo. |
| `kernel/gdt.h` / `kernel/gdt.c` | Define + load a flat 3-entry GDT (null, ring-0 code, ring-0 data). |
| `kernel/gdt_flush.asm` | `lgdt` + reload segment registers + far-jump to reload CS. |
| `kernel/idt.h` / `kernel/idt.c` | 256-entry IDT, `idt_set_gate`, `idt_init`. |
| `kernel/idt_load.asm` | `lidt` wrapper. |
| `kernel/isr.h` / `kernel/isr.c` | C-side exception handler, IRQ dispatcher, `register_t` struct, handler registration. |
| `kernel/isr_stubs.asm` | 32 ISR stubs (exceptions) + 16 IRQ stubs + common save/restore trampolines. |
| `kernel/pic.h` / `kernel/pic.c` | Remap 8259 PIC to vectors 0x20–0x2F, mask/unmask IRQs, send EOI. |
| `kernel/pit.h` / `kernel/pit.c` | Program PIT channel 0 to a frequency, count ticks on IRQ0. |
| `scripts/test.sh` | Boot kernel headless in QEMU, capture serial to a temp file, grep for the expected marker with a timeout. |
| `grub/grub.cfg` | GRUB menu entry that `multiboot`-loads the kernel (for the ISO deliverable). |

---

## Task 0: Toolchain & environment setup

**Files:**
- Modify: `.gitignore`

Install the cross-compiler and ISO tooling, then verify everything is callable. This task installs software and commits only the `.gitignore` change (no build artifacts).

- [ ] **Step 1: Install the toolchain via Homebrew**

Run:
```bash
brew install i686-elf-gcc i686-elf-binutils i686-elf-grub xorriso nasm qemu
```
(`nasm` and `qemu` may already be installed — `brew` will report "already installed", which is fine.)

- [ ] **Step 2: Verify every required binary is on PATH**

Run:
```bash
for t in i686-elf-gcc i686-elf-ld nasm qemu-system-i386 i686-elf-grub-mkrescue xorriso make; do
  printf "%-26s " "$t:"; command -v "$t" || echo "MISSING"; done
```
Expected: every line prints a path, none print `MISSING`.

> Note: Homebrew names GRUB's tool `i686-elf-grub-mkrescue` (not plain `grub-mkrescue`). The Makefile's `iso` target uses that exact name.

- [ ] **Step 3: Confirm the compiler targets ELF i386**

Run:
```bash
i686-elf-gcc -dumpmachine
```
Expected: `i686-elf`.

- [ ] **Step 4: Update `.gitignore` to ignore build output**

The current `.gitignore` contains `context.md` and `.claude`. Append the build directory and ISO output. Replace the file contents with:
```gitignore
context.md
.claude
build/
*.iso
.DS_Store
```

- [ ] **Step 5: Commit**

```bash
git add .gitignore
git commit -m "build: ignore build artifacts and ISO output"
```

---

## Task 1: Build pipeline + Multiboot boot + serial "BOOT_OK"

This task stands up the entire compile→link→boot→serial→test pipeline with the smallest possible kernel: the Multiboot stub calls `kernel_main`, which prints `BOOT_OK` to serial and exits QEMU. Proving this end-to-end first de-risks everything after it.

**Files:**
- Create: `include/types.h`, `include/io.h`
- Create: `kernel/serial.h`, `kernel/serial.c`
- Create: `kernel/kernel.c`
- Create: `boot/multiboot.asm`
- Create: `linker.ld`
- Create: `Makefile`
- Create: `scripts/test.sh`

- [ ] **Step 1: Create `include/types.h`**

```c
#pragma once
// Freestanding fixed-width types. i686-elf-gcc provides these headers even
// without a libc because they are mandated for freestanding C.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
```

- [ ] **Step 2: Create `include/io.h`**

```c
#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
// Tiny delay by writing to an unused port (POST checkpoint port 0x80).
static inline void io_wait(void) {
    outb(0x80, 0);
}
```

- [ ] **Step 3: Create `kernel/serial.h`**

```c
#pragma once
#include "../include/types.h"

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
```

- [ ] **Step 4: Create `kernel/serial.c`**

```c
#include "serial.h"
#include "../include/io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); // disable interrupts
    outb(COM1 + 3, 0x80); // enable DLAB (baud divisor)
    outb(COM1 + 0, 0x03); // divisor low  = 3 (38400 baud)
    outb(COM1 + 1, 0x00); // divisor high = 0
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static int serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    if (c == '\n') {            // make raw newlines render on terminals
        while (!serial_tx_empty()) {}
        outb(COM1, '\r');
    }
    while (!serial_tx_empty()) {}
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    for (; *s; s++) serial_putc(*s);
}
```

- [ ] **Step 5: Create `kernel/kernel.c` (minimal version)**

```c
#include "../include/types.h"
#include "../include/io.h"
#include "serial.h"

// Cleanly terminate QEMU when the isa-debug-exit device is present
// (used by the smoke test). On real hardware / plain GRUB boot this
// writes to an unused port and is harmless; execution falls through
// to the halt loop below.
static void qemu_exit(uint8_t code) {
    outb(0xF4, code);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    serial_write("BOOT_OK\n");

    if (magic != 0x2BADB002) {
        serial_write("BAD_MAGIC\n");
    }

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
}
```

> `0x2BADB002` is the magic value the bootloader (GRUB or QEMU's `-kernel`) leaves in `eax` to prove a Multiboot1-compliant boot.

- [ ] **Step 6: Create `boot/multiboot.asm`**

```nasm
; Multiboot1 header + kernel entry point.
bits 32

MBALIGN  equ 1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1            ; provide a memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002        ; Multiboot1 magic
CHECKSUM equ -(MAGIC + MBFLAGS)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top         ; set up the stack (grows downward)
    push ebx                   ; arg2: pointer to multiboot info struct
    push eax                   ; arg1: multiboot magic (0x2BADB002)
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang
```

- [ ] **Step 7: Create `linker.ld`**

```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;                    /* load the kernel at 1 MiB */

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)          /* header must be in the first 8 KiB */
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
}
```

- [ ] **Step 8: Create `Makefile`**

```makefile
# ---- Toolchain ----
CC      := i686-elf-gcc
AS      := nasm
QEMU    := qemu-system-i386

# ---- Flags ----
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude -Ikernel
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib

# ---- Sources ----
C_SRCS  := $(wildcard kernel/*.c)
ASM_SRCS:= $(wildcard boot/*.asm) $(wildcard kernel/*.asm)

C_OBJS  := $(patsubst %.c,build/%.o,$(C_SRCS))
ASM_OBJS:= $(patsubst %.asm,build/%.o,$(ASM_SRCS))
OBJS    := $(ASM_OBJS) $(C_OBJS)

KERNEL  := build/kernel.bin
ISO     := build/os.iso

EXPECT  ?= TIMER_OK

.PHONY: all run test iso clean

all: $(KERNEL)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -lgcc -o $@

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -serial stdio -display none -no-reboot

test: $(KERNEL)
	./scripts/test.sh $(EXPECT)

iso: $(KERNEL)
	@mkdir -p build/isodir/boot/grub
	cp $(KERNEL) build/isodir/boot/kernel.bin
	cp grub/grub.cfg build/isodir/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) build/isodir

clean:
	rm -rf build
```

> The link line places ASM objects before C objects and links `-lgcc` last so compiler-runtime helpers resolve.

- [ ] **Step 9: Create `scripts/test.sh`**

```bash
#!/usr/bin/env bash
# Boot the kernel headless in QEMU, capture COM1 serial output, and pass
# iff the expected marker appears within the timeout.
#
# Usage: scripts/test.sh <EXPECTED_MARKER> [timeout_seconds]
set -u

KERNEL="build/kernel.bin"
EXPECT="${1:?usage: test.sh <EXPECTED_MARKER> [timeout_seconds]}"
TIMEOUT="${2:-20}"

if [[ ! -f "$KERNEL" ]]; then
  echo "FAIL: $KERNEL not built (run 'make' first)"; exit 2
fi

LOG="$(mktemp)"
qemu-system-i386 -kernel "$KERNEL" \
  -serial "file:$LOG" \
  -display none -no-reboot \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  >/dev/null 2>&1 &
QPID=$!

# Poll up to TIMEOUT seconds for QEMU to exit on its own (isa-debug-exit).
for ((i = 0; i < TIMEOUT * 10; i++)); do
  kill -0 "$QPID" 2>/dev/null || break
  sleep 0.1
done
# If still running (e.g. kernel hung), kill it.
if kill -0 "$QPID" 2>/dev/null; then
  kill "$QPID" 2>/dev/null
fi
wait "$QPID" 2>/dev/null

echo "----- serial output -----"
cat "$LOG"
echo "-------------------------"

if grep -q "$EXPECT" "$LOG"; then
  echo "PASS: found '$EXPECT'"
  rm -f "$LOG"; exit 0
else
  echo "FAIL: '$EXPECT' not found in serial output"
  rm -f "$LOG"; exit 1
fi
```

- [ ] **Step 10: Make the test script executable**

Run:
```bash
chmod +x scripts/test.sh
```

- [ ] **Step 11: Build the kernel**

Run:
```bash
make
```
Expected: compiles with no errors; produces `build/kernel.bin`. Verify it is a valid Multiboot1 image:
```bash
i686-elf-grub-file --is-x86-multiboot build/kernel.bin && echo "multiboot OK"
```
Expected: prints `multiboot OK`.

- [ ] **Step 12: Run the smoke test**

Run:
```bash
make test EXPECT=BOOT_OK
```
Expected: serial output contains `BOOT_OK`; script prints `PASS: found 'BOOT_OK'` and exits 0.

- [ ] **Step 13: Commit**

```bash
git add include kernel boot linker.ld Makefile scripts
git commit -m "feat(boot): multiboot stub, serial output, and QEMU test harness"
```

---

## Task 2: VGA text-mode terminal

Add an on-screen text terminal so the kernel is visible in the QEMU window (the actual demo surface), in addition to serial. Verified by a new `VGA_OK` serial marker printed right after VGA init.

**Files:**
- Create: `kernel/vga.h`, `kernel/vga.c`
- Modify: `kernel/kernel.c`

- [ ] **Step 1: Create `kernel/vga.h`**

```c
#pragma once
#include "../include/types.h"

// VGA text-mode colors (low nibble = foreground, high nibble = background).
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE = 1, VGA_GREEN = 2, VGA_CYAN = 3,
    VGA_RED = 4, VGA_MAGENTA = 5, VGA_BROWN = 6, VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8, VGA_LIGHT_BLUE = 9, VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11, VGA_LIGHT_RED = 12, VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14, VGA_WHITE = 15,
};

void vga_init(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_putc(char c);
void vga_write(const char *s);
```

- [ ] **Step 2: Create `kernel/vga.c`**

```c
#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
static volatile uint16_t *const VGA_MEM = (uint16_t *)0xB8000;

static size_t row, col;
static uint8_t color;

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}

void vga_init(void) {
    row = 0;
    col = 0;
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        if (++row == VGA_HEIGHT) vga_scroll();
        return;
    }
    VGA_MEM[row * VGA_WIDTH + col] = vga_entry(c, color);
    if (++col == VGA_WIDTH) {
        col = 0;
        if (++row == VGA_HEIGHT) vga_scroll();
    }
}

void vga_write(const char *s) {
    for (; *s; s++) vga_putc(*s);
}
```

- [ ] **Step 3: Wire VGA into `kernel/kernel.c`**

Replace the entire contents of `kernel/kernel.c` with:
```c
#include "../include/types.h"
#include "../include/io.h"
#include "serial.h"
#include "vga.h"

static void qemu_exit(uint8_t code) {
    outb(0xF4, code);
}

// Print to both the on-screen terminal and the serial port.
static void klog(const char *s) {
    vga_write(s);
    serial_write(s);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    serial_write("BOOT_OK\n");

    vga_init();
    vga_set_color(VGA_WHITE, VGA_BLUE);
    klog("omen OS - phase 1\n");
    klog("VGA_OK\n");

    if (magic != 0x2BADB002) {
        klog("BAD_MAGIC\n");
    }

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
}
```

- [ ] **Step 4: Build**

Run: `make`
Expected: compiles cleanly; `build/kernel.bin` regenerated.

- [ ] **Step 5: Smoke test the new marker**

Run: `make test EXPECT=VGA_OK`
Expected: `PASS: found 'VGA_OK'`.

- [ ] **Step 6: (Optional) eyeball it**

Run: `make run` — a QEMU window shows white-on-blue `omen OS - phase 1` / `VGA_OK`. Close the window or Ctrl-C to quit.

- [ ] **Step 7: Commit**

```bash
git add kernel/vga.h kernel/vga.c kernel/kernel.c
git commit -m "feat(vga): 80x25 text-mode terminal with scroll"
```

---

## Task 3: Global Descriptor Table (flat ring-0 segments)

Install our own GDT (GRUB/QEMU left one we don't control). This is a prerequisite for the IDT, whose gates reference the kernel code selector `0x08`. Verified by `GDT_OK`.

**Files:**
- Create: `kernel/gdt.h`, `kernel/gdt.c`, `kernel/gdt_flush.asm`
- Modify: `kernel/kernel.c`

- [ ] **Step 1: Create `kernel/gdt.h`**

```c
#pragma once
#include "../include/types.h"

void gdt_init(void);
```

- [ ] **Step 2: Create `kernel/gdt.c`**

```c
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gp;

extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_gate(int n, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[n].base_low    = base & 0xFFFF;
    gdt[n].base_mid    = (base >> 16) & 0xFF;
    gdt[n].base_high   = (base >> 24) & 0xFF;
    gdt[n].limit_low   = limit & 0xFFFF;
    gdt[n].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].access      = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                  // null descriptor
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);   // ring-0 code: present, exec/read
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);   // ring-0 data: present, read/write

    gdt_flush((uint32_t)&gp);
}
```

> `0x9A` = present|ring0|code|readable. `0x92` = present|ring0|data|writable. `0xCF` = 4 KiB granularity + 32-bit + top limit nibble.

- [ ] **Step 3: Create `kernel/gdt_flush.asm`**

```nasm
bits 32
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]   ; pointer to gdt_ptr
    lgdt [eax]

    mov ax, 0x10         ; data selector (entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.reload_cs  ; code selector (entry 1) - far jump reloads CS
.reload_cs:
    ret
```

- [ ] **Step 4: Call `gdt_init` from `kernel/kernel.c`**

In `kernel/kernel.c`, add the include near the other includes:
```c
#include "gdt.h"
```
Then, in `kernel_main`, immediately after the `VGA_OK` log block and before the `magic` check, insert:
```c
    gdt_init();
    klog("GDT_OK\n");
```

- [ ] **Step 5: Build**

Run: `make`
Expected: compiles cleanly.

- [ ] **Step 6: Smoke test**

Run: `make test EXPECT=GDT_OK`
Expected: `PASS: found 'GDT_OK'` (proves we executed past `lgdt` and the CS far-jump without faulting).

- [ ] **Step 7: Commit**

```bash
git add kernel/gdt.h kernel/gdt.c kernel/gdt_flush.asm kernel/kernel.c
git commit -m "feat(gdt): flat ring-0 GDT with segment reload"
```

---

## Task 4: IDT + ISR stubs + exception handler

Install a 256-entry IDT and wire CPU exception vectors 0–31 to assembly stubs that funnel into a single C handler. Verified two ways: `IDT_OK` after load, and `ISR3_OK` after deliberately triggering a breakpoint (`int 3`) to prove the dispatch path actually runs.

**Files:**
- Create: `kernel/idt.h`, `kernel/idt.c`, `kernel/idt_load.asm`
- Create: `kernel/isr.h`, `kernel/isr.c`, `kernel/isr_stubs.asm`
- Modify: `kernel/kernel.c`

- [ ] **Step 1: Create `kernel/idt.h`**

```c
#pragma once
#include "../include/types.h"

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
```

- [ ] **Step 2: Create `kernel/isr.h`**

```c
#pragma once
#include "../include/types.h"

// Register snapshot pushed by the common stub. Field order MUST match the
// push sequence in isr_stubs.asm (see comments there).
typedef struct registers {
    uint32_t ds;                                     // pushed by stub
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha
    uint32_t int_no, err_code;                       // pushed by stub
    uint32_t eip, cs, eflags, useresp, ss;           // pushed by CPU
} registers_t;

typedef void (*isr_handler_t)(registers_t *);

void isr_install(void);                          // register exception gates 0-31
void irq_install_handler(int irq, isr_handler_t h);
void irq_uninstall_handler(int irq);

// Called from assembly stubs.
void isr_handler(registers_t *r);
void irq_handler(registers_t *r);
```

- [ ] **Step 3: Create `kernel/idt.c`**

```c
#include "idt.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   ip;

extern void idt_load(uint32_t idt_ptr_addr);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0, 0);   // all zero = not present
    idt_load((uint32_t)&ip);
}
```

- [ ] **Step 4: Create `kernel/idt_load.asm`**

```nasm
bits 32
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
```

- [ ] **Step 5: Create `kernel/isr_stubs.asm`**

```nasm
bits 32

extern isr_handler
extern irq_handler

; Exceptions WITHOUT a CPU-pushed error code: push a dummy 0 so the stack
; layout is uniform, then the vector number.
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

; Exceptions WITH a CPU-pushed error code: only push the vector number.
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common_stub
%endmacro

; Hardware IRQs: push dummy error code, then the IDT vector number.
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

; CPU exceptions 0-31. 8,10,11,12,13,14,17 push an error code.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; Hardware IRQs 0-15 remapped to IDT vectors 32-47.
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; Save full state, call C handler with a pointer to the registers, restore.
; Push order here defines the registers_t layout in isr.h.
isr_common_stub:
    pusha                  ; edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds
    push eax               ; save data segment selector
    mov ax, 0x10           ; load kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp               ; arg: registers_t*
    call isr_handler
    add esp, 4             ; pop the argument
    pop eax                ; restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8             ; pop error code + vector number
    sti
    iret

irq_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call irq_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret
```

- [ ] **Step 6: Create `kernel/isr.c`**

```c
#include "isr.h"
#include "idt.h"
#include "serial.h"
#include "vga.h"

// Declared in isr_stubs.asm.
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

static void (*const isr_stubs[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static const char *const exception_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound range", "Invalid opcode", "Device N/A",
    "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection", "Page fault", "Reserved",
    "x87 FPU error", "Alignment check", "Machine check", "SIMD FP",
    "Virtualization", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
};

static isr_handler_t irq_handlers[16] = { 0 };

static void klog(const char *s) { vga_write(s); serial_write(s); }

void isr_install(void) {
    for (int i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, (uint32_t)isr_stubs[i], 0x08, 0x8E);
}

void irq_install_handler(int irq, isr_handler_t h) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = h;
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = 0;
}

void isr_handler(registers_t *r) {
    if (r->int_no == 3) {            // breakpoint: used by the smoke test
        klog("ISR3_OK\n");
        return;                      // resume normally
    }
    klog("EXCEPTION: ");
    if (r->int_no < 32) klog(exception_names[r->int_no]);
    klog("\nHALT\n");
    for (;;) { __asm__ volatile ("cli; hlt"); }
}

// PIC end-of-interrupt is sent here; declared in pic.h (Task 5).
extern void pic_send_eoi(uint8_t irq);

void irq_handler(registers_t *r) {
    uint8_t irq = (uint8_t)(r->int_no - 32);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);
    pic_send_eoi(irq);
}
```

> `irq_handler` references `pic_send_eoi`, which is implemented in Task 5. Tasks must be done in order; do not build between this step and Task 5's completion (the IRQ path is only exercised starting in Task 6).

- [ ] **Step 7: Wire IDT/ISR into `kernel/kernel.c`**

Add includes near the others:
```c
#include "idt.h"
#include "isr.h"
```
In `kernel_main`, immediately after the `GDT_OK` block, insert:
```c
    idt_init();
    isr_install();
    klog("IDT_OK\n");

    __asm__ volatile ("int $0x3");   // breakpoint -> should print ISR3_OK
```

> At this point the file still references `pic`/`pit` symbols only if you added them — do not add Task 5/6 code yet. Build happens in Step 8 with just IDT/ISR.

- [ ] **Step 8: Build**

Run: `make`

> If the linker reports an undefined reference to `pic_send_eoi`, that is expected only if you skipped ahead. With the code exactly as written above, `pic_send_eoi` is referenced by `isr.c` but **not yet defined** — so this build WILL fail at link time. To keep Task 4 independently testable, temporarily add a stub at the bottom of `kernel/isr.c`:
> ```c
> // TEMPORARY stub, replaced by pic.c in Task 5.
> __attribute__((weak)) void pic_send_eoi(uint8_t irq) { (void)irq; }
> ```
> Then `make` should link cleanly.

Expected: compiles and links; `build/kernel.bin` produced.

- [ ] **Step 9: Smoke test IDT load**

Run: `make test EXPECT=IDT_OK`
Expected: `PASS: found 'IDT_OK'`.

- [ ] **Step 10: Smoke test the exception dispatch path**

Run: `make test EXPECT=ISR3_OK`
Expected: `PASS: found 'ISR3_OK'` (proves `int $0x3` reached the assembly stub, hit the C handler, and returned via `iret`).

- [ ] **Step 11: Commit**

```bash
git add kernel/idt.h kernel/idt.c kernel/idt_load.asm \
        kernel/isr.h kernel/isr.c kernel/isr_stubs.asm kernel/kernel.c
git commit -m "feat(idt): IDT, ISR stubs, and exception dispatch (int3 verified)"
```

---

## Task 5: PIC remap (8259)

Remap the master/slave PICs from their default vectors (which overlap CPU exceptions) to 0x20–0x2F, register IRQ gates 32–47 in the IDT, and provide `pic_send_eoi`. This replaces the temporary `pic_send_eoi` stub from Task 4. Verified by `PIC_OK`.

**Files:**
- Create: `kernel/pic.h`, `kernel/pic.c`
- Modify: `kernel/isr.c` (remove the temporary stub; register IRQ gates), `kernel/kernel.c`

- [ ] **Step 1: Create `kernel/pic.h`**

```c
#pragma once
#include "../include/types.h"

void pic_remap(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
```

- [ ] **Step 2: Create `kernel/pic.c`**

```c
#include "pic.h"
#include "../include/io.h"

#define PIC1         0x20
#define PIC2         0xA0
#define PIC1_CMD     PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_CMD     PIC2
#define PIC2_DATA    (PIC2 + 1)
#define PIC_EOI      0x20

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

void pic_remap(void) {
    uint8_t a1 = inb(PIC1_DATA);   // save masks
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();      // master vector offset -> 0x20
    outb(PIC2_DATA, 0x28); io_wait();      // slave vector offset  -> 0x28
    outb(PIC1_DATA, 0x04); io_wait();      // tell master: slave at IRQ2
    outb(PIC2_DATA, 0x02); io_wait();      // tell slave its cascade identity
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, a1);                   // restore masks
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}
```

- [ ] **Step 3: Remove the temporary stub and register IRQ gates in `kernel/isr.c`**

First, delete the temporary stub you added in Task 4 Step 8 (the `__attribute__((weak)) void pic_send_eoi...` line at the bottom of `kernel/isr.c`).

Then add the IRQ stub externs and an IRQ-gate installer. Add these externs alongside the existing `isr0..isr31` externs:
```c
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);
```
Add the stub table after `isr_stubs[]`:
```c
static void (*const irq_stubs[16])(void) = {
    irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15,
};
```
Add a new public installer. First declare it in `kernel/isr.h` (add to the existing declarations):
```c
void irq_install(void);   // register IRQ gates 32-47 (call after pic_remap)
```
Then implement it in `kernel/isr.c`:
```c
void irq_install(void) {
    for (int i = 0; i < 16; i++)
        idt_set_gate((uint8_t)(32 + i), (uint32_t)irq_stubs[i], 0x08, 0x8E);
}
```

- [ ] **Step 4: Wire PIC into `kernel/kernel.c`**

Add include:
```c
#include "pic.h"
```
In `kernel_main`, immediately after the `int $0x3` line, insert:
```c
    pic_remap();
    irq_install();
    klog("PIC_OK\n");
```

- [ ] **Step 5: Build**

Run: `make`
Expected: compiles and links cleanly (the real `pic_send_eoi` now satisfies the reference from `isr.c`).

- [ ] **Step 6: Smoke test**

Run: `make test EXPECT=PIC_OK`
Expected: `PASS: found 'PIC_OK'`.

- [ ] **Step 7: Commit**

```bash
git add kernel/pic.h kernel/pic.c kernel/isr.c kernel/isr.h kernel/kernel.c
git commit -m "feat(pic): remap 8259 PICs to 0x20-0x2F and install IRQ gates"
```

---

## Task 6: PIT timer (IRQ0) + enable interrupts

Program PIT channel 0 to fire IRQ0 at 100 Hz, count ticks, unmask IRQ0, and enable interrupts. The kernel waits for a number of ticks, then prints the final `TIMER_OK` marker. This is the capstone proving the whole interrupt pipeline (PIT → PIC → IDT → IRQ stub → C handler → EOI) works end to end.

**Files:**
- Create: `kernel/pit.h`, `kernel/pit.c`
- Modify: `kernel/kernel.c`

- [ ] **Step 1: Create `kernel/pit.h`**

```c
#pragma once
#include "../include/types.h"

void pit_init(uint32_t frequency);
uint32_t pit_ticks(void);
```

- [ ] **Step 2: Create `kernel/pit.c`**

```c
#include "pit.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182

static volatile uint32_t ticks = 0;

static void pit_callback(registers_t *r) {
    (void)r;
    ticks++;
}

uint32_t pit_ticks(void) {
    return ticks;
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_HZ / frequency;

    outb(PIT_COMMAND, 0x36);                    // channel 0, lo/hi, mode 3 (square wave)
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, pit_callback);
    pic_clear_mask(0);                          // unmask IRQ0
}
```

- [ ] **Step 3: Wire PIT + interrupt enable + demo into `kernel/kernel.c`**

Add include:
```c
#include "pit.h"
```
In `kernel_main`, replace the existing trailing block:
```c
    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
```
with:
```c
    pit_init(100);                    // 100 Hz -> tick every 10 ms
    __asm__ volatile ("sti");         // enable hardware interrupts
    klog("PIT_OK\n");

    // Wait for ~30 ticks (~300 ms) to prove the timer IRQ actually fires.
    while (pit_ticks() < 30) {
        __asm__ volatile ("hlt");     // sleep until the next interrupt
    }
    klog("TIMER_OK\n");

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
```

- [ ] **Step 4: Build**

Run: `make`
Expected: compiles and links cleanly.

- [ ] **Step 5: Smoke test the timer (the default `make test` target)**

Run: `make test`
Expected: serial output ends with `PIT_OK` then `TIMER_OK`; script prints `PASS: found 'TIMER_OK'`. (Default `EXPECT` is `TIMER_OK`.)

- [ ] **Step 6: Verify the full marker sequence in one boot**

Run:
```bash
make run
```
Watch serial stdout (and the QEMU window). Expected order:
```
BOOT_OK
VGA_OK
GDT_OK
IDT_OK
ISR3_OK
PIC_OK
PIT_OK
TIMER_OK
```
Then QEMU exits (isa-debug-exit). Press Ctrl-C if it does not exit on your setup.

- [ ] **Step 7: Commit**

```bash
git add kernel/pit.h kernel/pit.c kernel/kernel.c
git commit -m "feat(pit): 100Hz timer tick via IRQ0, full interrupt path verified"
```

---

## Task 7: GRUB bootable ISO deliverable

Produce a real GRUB-booted `.iso` so the project has an authentic bootloader artifact (the README advertises GRUB2). The dev loop still uses `make test` / `make run` with QEMU `-kernel`; this is the demo/submission artifact.

**Files:**
- Create: `grub/grub.cfg`
- (Makefile `iso` target already added in Task 1.)

- [ ] **Step 1: Create `grub/grub.cfg`**

```
set timeout=0
set default=0

menuentry "omen OS (phase 1)" {
    multiboot /boot/kernel.bin
    boot
}
```

- [ ] **Step 2: Build the ISO**

Run:
```bash
make iso
```
Expected: produces `build/os.iso` with no errors. (`i686-elf-grub-mkrescue` may print warnings about missing optional modules — those are fine as long as `build/os.iso` is created.)

- [ ] **Step 3: Boot the ISO in QEMU and verify GRUB hands off**

Run:
```bash
qemu-system-i386 -cdrom build/os.iso -serial stdio -display none -no-reboot \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04
```
Expected: same marker sequence ending in `TIMER_OK`, this time loaded by GRUB from the ISO rather than QEMU's `-kernel`. QEMU exits via isa-debug-exit.

- [ ] **Step 4: Commit**

```bash
git add grub/grub.cfg
git commit -m "build(iso): GRUB grub.cfg and bootable ISO target"
```

---

## Task 8: Documentation & Phase 1 close-out

Update project docs to reflect the now-working build and mark Phase 1 complete.

**Files:**
- Modify: `README.md`
- Modify: `project/context.md` (status checkbox)

> Note on `context.md`: it is git-ignored (it appears in `.gitignore`), so editing it updates the working tree for your own tracking but will **not** be committed. That is intentional per the existing repo setup. Update it anyway so local status stays accurate.

- [ ] **Step 1: Update `README.md` build/run instructions**

In `README.md`, replace the `## ➢ Building and running` section body with:
```markdown
Requires the `i686-elf` cross-compiler toolchain, NASM, and QEMU:

\`\`\`bash
brew install i686-elf-gcc i686-elf-binutils i686-elf-grub xorriso nasm qemu
\`\`\`

Build and run (QEMU loads the Multiboot kernel directly):

\`\`\`bash
make            # build build/kernel.bin
make run        # boot in QEMU (serial on stdout, VGA in the window)
make test       # headless boot; passes if the timer tick is reached
\`\`\`

Build a GRUB bootable ISO (the "real bootloader" artifact):

\`\`\`bash
make iso        # produces build/os.iso
qemu-system-i386 -cdrom build/os.iso
\`\`\`
```
(Remove the backslashes before the triple backticks — they are escaping artifacts of this plan document; the real README uses plain ``` fences.)

- [ ] **Step 2: Mark Phase 1 complete in `project/context.md`**

In `project/context.md`, under `## Status`, change:
```markdown
- [ ] Phase 1 — Boot infrastructure (not started)
```
to:
```markdown
- [x] Phase 1 — Boot infrastructure (Boot, GDT, IDT/ISR/PIC, PIT timer) — DONE
```
And update the final line `Toolchain/environment not yet set up. Nothing built yet.` to:
```markdown
Toolchain installed (i686-elf-gcc, nasm, qemu, grub). Phase 1 builds and boots:
`make test` passes (BOOT→GDT→IDT→PIC→PIT→TIMER_OK). Phase 2 (drivers & memory) next.
```

- [ ] **Step 3: Final verification before committing docs**

Run:
```bash
make clean && make && make test
```
Expected: clean rebuild from scratch, then `PASS: found 'TIMER_OK'`.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: Phase 1 build/run instructions and status"
```

---

## Phase 1 Definition of Done

- [ ] `make clean && make` builds `build/kernel.bin` from scratch with no warnings.
- [ ] `i686-elf-grub-file --is-x86-multiboot build/kernel.bin` confirms a valid Multiboot1 image.
- [ ] `make test` passes (`TIMER_OK` on serial), exercising the full chain: Multiboot boot → serial → VGA → GDT load → IDT load → exception dispatch (`int 3`) → PIC remap → PIT IRQ0 tick → EOI.
- [ ] `make iso` produces `build/os.iso` that boots under GRUB to the same `TIMER_OK`.
- [ ] All eight markers appear in order on a single `make run`: `BOOT_OK, VGA_OK, GDT_OK, IDT_OK, ISR3_OK, PIC_OK, PIT_OK, TIMER_OK`.
- [ ] Each task is committed separately under `Aarohan Niraula <aarohan567@gmail.com>` on branch `bootloader`.

---

## Notes for the implementer (gotchas)

1. **Task ordering is load-bearing.** `isr.c` references `pic_send_eoi` before Task 5 defines it; that's why Task 4 Step 8 adds a temporary weak stub and Task 5 Step 3 removes it. Don't skip those steps.
2. **`registers_t` field order must match `isr_stubs.asm` exactly.** If you reorder either, the C handler reads garbage. The struct's first field (`ds`) is the last thing pushed before `push esp`.
3. **`-ffreestanding` is mandatory** — there is no libc. Don't `#include <stdio.h>` or call `printf`; use `serial_write`/`vga_write`.
4. **Homebrew GRUB tool name** is `i686-elf-grub-mkrescue`, not `grub-mkrescue`. macOS's system GRUB (if any) targets the wrong format; always use the `i686-elf-` prefixed tools.
5. **QEMU under TCG on arm64 is slower** than KVM but fully correct for i386. The 20-second test timeout is generous; if a test times out, the kernel almost certainly faulted/hung — inspect the serial log the script prints.
6. **isa-debug-exit exit code is always odd** (`(value<<1)|1`); never assert on QEMU's exit code being 0. The test script asserts on the serial marker, not the exit code.
7. **Interrupts stay masked until Task 6.** Only `pit_init` unmasks IRQ0 and only `kernel_main` runs `sti`. Until then, a stray IRQ won't fire, which is why earlier tasks are safe.

---

## Self-review

- **Spec coverage:** Boot (Task 1) ✓, GDT (Task 3) ✓, IDT/ISR (Task 4) ✓, PIC remap (Task 5) ✓, PIT timer (Task 6) ✓ — roadmap items 1–4 all covered. Repo structure (`boot/ kernel/ Makefile linker.ld`) created ✓; `drivers/ mm/ sched/` are Phase 2/3 and intentionally not created here. Toolchain-from-brew per context.md ✓. QEMU testing ✓. GRUB ISO deliverable ✓.
- **Placeholders:** none — every code step shows complete code; every command shows expected output.
- **Type consistency:** `registers_t` defined once in `isr.h` and used consistently; `pic_send_eoi(uint8_t)`, `irq_install_handler(int, isr_handler_t)`, `pit_ticks(void)` signatures match across declaration and use; marker strings (`BOOT_OK`…`TIMER_OK`) consistent between kernel code and test invocations.
