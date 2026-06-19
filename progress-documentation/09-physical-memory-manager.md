# 09 — Physical memory manager

The PMM tracks which 4 KiB chunks of physical RAM are in use. It's the lowest layer
of memory management — everything that allocates memory (kmalloc, page tables) goes
through it.

## What "physical memory" means here

The CPU eventually addresses real RAM chips. Each 4 KiB block of that RAM is a
**frame**. The PMM's only job is to know which frames are free and hand them out on
request. It doesn't know about virtual addresses or processes — that's paging's job.

## Bitmap allocator

We track frames with a bitmap: one bit per frame. For 128 MiB of RAM that's
128 MiB / 4 KiB = 32,768 frames, which takes 32,768 bits = 4 KiB for the bitmap.
Small enough to put in static storage.

Allocation scans the bitmap word-by-word (32 bits at a time), skipping fully-used
words in one comparison, then finds the first free bit within a word. Returns the
frame's physical address (`frame_index * 4096`), or 0 on OOM.

## Reading the memory map from GRUB

GRUB passes a memory map to `kernel_main` that tells us which regions of RAM are
actually usable — some addresses are reserved for hardware, ACPI data, etc. We walk
that map and call `pmm_mark_free()` on type-1 (available) regions only.

After marking available frames free, we re-reserve two things:
1. Frame 0 (address `0x000000`) — the null-pointer page. Keeping it reserved means
   a null dereference causes an immediate fault instead of silently reading garbage.
2. The kernel image itself — from 1 MiB (where GRUB loaded us) up to `_kernel_end`
   (a symbol the linker script exports at the end of the kernel binary).

## Output

```
PMM: 32615 free frames (130460 KiB)
PMM_OK
```

The count depends on how much RAM QEMU is configured with (default 128 MiB).
