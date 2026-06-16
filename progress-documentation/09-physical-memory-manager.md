# 09 — Physical memory manager: bitmap allocator

**What we built:** a **physical memory manager (PMM)** that reads the multiboot
memory map provided by GRUB, tracks which 4 KiB page frames are free or used via
a bitmap, and exposes `pmm_alloc_frame()` / `pmm_free_frame()` so the rest of
the kernel can request and return physical pages.

## The big idea: physical vs virtual memory

The CPU eventually accesses real DRAM chips. Each contiguous 4 KiB chunk of
physical DRAM is a **frame**. The PMM's only job is to track which frames are
available. It does *not* know about virtual addresses or processes — that is
paging's concern. The PMM is the lowest layer of memory management.

## Why a bitmap?

A bitmap stores one bit per frame. For 1 GiB of RAM:

```
1 GiB / 4 KiB = 262,144 frames → 262,144 bits = 32,768 bytes (32 KiB bitmap)
```

32 KiB of static BSS is tiny and gives O(n/32) worst-case scan using 32-bit
words. More sophisticated structures (buddy system, slab) are only needed once
we have a heap — and the heap depends on the PMM, not the other way around.

## Reading the multiboot memory map

GRUB sets flag bit 6 in the multiboot info struct and fills in `mmap_addr` /
`mmap_length`. Each entry is a variable-length `mb_mmap_t`:

| Field     | Meaning |
|-----------|---------|
| `size`    | bytes in this entry (excluding the size field itself) |
| `addr`    | base physical address (64-bit) |
| `len`     | region length (64-bit) |
| `type`    | 1 = available RAM, anything else = reserved/ACPI/bad |

We walk the map, calling `pmm_mark_free()` for type-1 regions. We only handle
`addr_hi == 0` entries (first 4 GiB) since we are a 32-bit kernel.

## What gets reserved

After freeing available RAM we re-reserve:

1. **Frame 0** (physical address `0x000000`) — the null-pointer page.
   Keeping it mapped-but-reserved means a null dereference faults immediately
   instead of silently reading uninitialised data.
2. **The kernel image** — from `0x100000` (1 MiB, where GRUB loads us) up to
   `_kernel_end` (a symbol injected by `linker.ld`).

## Allocation

`pmm_alloc_frame()` scans the bitmap word-by-word, skipping fully-used words in
one comparison, then does a bit scan within the first word that has a free bit.
Returns the physical address of the frame (frame_index × 4096), or `0` on OOM.

## Diagnostic output

```
PMM: 32615 free frames (130460 KiB)
PMM_OK
```

The first line is printed during `pmm_init()` via `serial_write_uint()`. The
exact number depends on QEMU's `-m` flag (default 128 MiB).

## Key files

| File | Role |
|------|------|
| `mm/pmm.c` | bitmap, mmap parser, alloc/free, diagnostic log |
| `mm/pmm.h` | `pmm_init()`, `pmm_alloc_frame()`, `pmm_free_frame()`, `pmm_free_count()` |
| `linker.ld` | exports `_kernel_end` symbol |
