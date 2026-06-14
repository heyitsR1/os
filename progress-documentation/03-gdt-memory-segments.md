# 03 — The GDT: telling the CPU how to view memory

**What we built:** a small table called the **GDT** that defines how the CPU
interprets memory addresses. It's mostly a formality for us, but a *required* one —
the interrupt system we build next depends on it.

## Why this exists at all (a bit of x86 history)

Old Intel CPUs couldn't address much memory, so they invented **segmentation**: memory
was divided into chunks ("segments"), and every address was interpreted relative to
some segment. To use a segment, the CPU looks it up in a table of **descriptors** —
each descriptor says "this segment starts here, is this big, and has these
permissions (readable? executable? which privilege level?)." That table is the
**Global Descriptor Table (GDT)**.

Modern OSes don't really *want* segmentation — it's an obsolete way to organize
memory (we use **paging** for that later, in Phase 2). But x86 in 32-bit protected
mode **forces** you to have a valid GDT; the CPU refuses to run without one. So the
standard move, which we follow, is to neutralize segmentation by making it do
nothing.

## The "flat model": segments that cover everything

We create just **three** descriptors:

| # | Selector | What it is | Covers |
|---|----------|-----------|--------|
| 0 | — | Null descriptor (mandatory, must exist, never used) | nothing |
| 1 | `0x08` | Ring-0 **code** segment | all 4 GiB of memory |
| 2 | `0x10` | Ring-0 **data** segment | all 4 GiB of memory |

Both real segments start at address 0 and span the entire 4 GiB address space. The
effect: a "segmented" address and a plain address become **identical**. Segmentation
is still technically on (the CPU insists), but it no longer changes anything. This is
called the **flat memory model** — one big flat space, the way you'd intuitively
expect memory to work.

- **"Ring 0"** = the most privileged CPU mode (full hardware access). Our entire OS
  runs in ring 0 — there's no user mode in this project, by design.
- **Code vs data segments** exist because the CPU wants to know which memory holds
  instructions vs. plain data. Both cover the same 4 GiB; they just carry different
  permission bits (executable vs. writable).
- **Selector** (`0x08`, `0x10`) is just the number the CPU uses to refer to a
  descriptor — an offset into the table. Remember `0x08` (code) — the interrupt
  table we build next points every entry at it.

## What the code does

- **`kernel/gdt.c`** builds the 3-entry table in memory. The fiddly bit is that an
  x86 descriptor packs its fields (base address, size limit, permission flags) into a
  weird, historically-bolted-on byte layout — the bits of one number are scattered
  across non-adjacent bytes. `gdt_set_gate()` does that bit-shuffling so we can
  specify each entry in human terms. The magic numbers `0x9A` (code permissions) and
  `0x92` (data permissions) are just those permission bits spelled out.

- **`kernel/gdt_flush.asm`** does the actual install. Loading a GDT is a privileged
  CPU instruction (`lgdt`), and afterward you must "refresh" the segment registers so
  the CPU starts using the new table. Refreshing the **code** segment specifically
  requires a trick — a **far jump** (`jmp 0x08:...`) — because you can't just write
  to the code-segment register directly; jumping to an address *via* the new selector
  forces the reload. This is a well-known x86 ritual; it's 15 lines of assembly we
  write once.

> The far jump is the part that looks strange ("why jump to the very next line?").
> The answer: the *act* of jumping through selector `0x08` is what makes the CPU
> adopt our new code segment. The destination barely matters; the mechanism is the
> point.

## How we know it worked

There's no visible output from a GDT — it's pure CPU plumbing. So our proof is
indirect but solid: right after installing the GDT, we print `GDT_OK`. If the table
or the far-jump were malformed, the CPU would **fault** (crash) at that instruction
and we'd never reach the print. Seeing `GDT_OK` on the serial log means the install
and the segment-register reload both succeeded.

## Where we are now

- ✅ Our own flat, ring-0 GDT is installed; segmentation is neutralized.
- ✅ Selector `0x08` (code) and `0x10` (data) are valid and live.
- ✅ `GDT_OK` confirms no fault during install.
- ⏭️ Next: the **IDT** (Interrupt Descriptor Table) plus exception handlers — the
  machinery that lets the CPU call *our* code when something happens (an error, a
  key press, a timer tick). Every IDT entry will reference that `0x08` code selector,
  which is exactly why the GDT had to come first.

**Jargon recap:** *segmentation* = an old x86 scheme for dividing memory; we neutralize
it. *GDT* = the table of segment descriptors the CPU requires. *flat model* = making
all segments span the whole address space so segmentation does nothing. *ring 0* = max
CPU privilege (where our whole OS lives). *selector* = the number identifying a GDT
entry (`0x08` code, `0x10` data). *far jump* = the assembly trick that forces the CPU
to adopt a new code segment.
