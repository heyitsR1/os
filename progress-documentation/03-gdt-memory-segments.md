# 03 — GDT (Global Descriptor Table)

The GDT is a table the CPU requires in 32-bit protected mode. It describes memory
"segments" — regions with permissions attached. We don't actually want segmentation
(it's an old x86 thing that modern OSes ignore in favor of paging), but the CPU
won't run without a valid GDT, so we set one up and make it do nothing.

## What we put in it

Three entries:

| Entry | Selector | What it is |
|-------|----------|-----------|
| 0 | — | Null descriptor (required, the CPU expects this to exist but never use it) |
| 1 | `0x08` | Ring-0 code segment, covers all 4 GiB |
| 2 | `0x10` | Ring-0 data segment, covers all 4 GiB |

Both the code and data segments start at address 0 and span the entire 4 GiB
address space. The effect: segmentation is technically active, but every "segmented"
address equals the raw address. It's called the flat memory model — one big flat
space with no translation happening. That's what we want.

Ring 0 is the most privileged CPU mode (full hardware access). Our whole OS runs
there — no user mode in this project.

The `0x08` and `0x10` selectors matter later: every IDT entry we build in the next
step will reference `0x08` to tell the CPU which segment the interrupt handler runs
in. That's why the GDT has to exist before we build the IDT.

## The code

`kernel/gdt.c` builds the 3-entry table. The annoying part is that an x86 descriptor
packs its fields (base address, size limit, permissions) into a weird historical byte
layout where bits of one number are scattered across non-adjacent bytes. `gdt_set_gate()`
handles that bit-shuffling.

`kernel/gdt_flush.asm` does the actual install. Loading a GDT requires the `lgdt`
instruction, and then you have to "refresh" the segment registers so the CPU starts
using the new table. Refreshing the code segment specifically requires a **far jump**
(`jmp 0x08:next_instruction`) — you can't write to the code segment register directly,
so jumping via the selector forces the reload. It looks strange (jumping to the next
line) but that's the standard x86 way to do it.

## How we know it worked

The GDT produces no visible output. Proof is indirect: we print `GDT_OK` immediately
after the install. If the table or far-jump were malformed the CPU would have faulted
and we'd never reach that print. Seeing it means the install succeeded.
