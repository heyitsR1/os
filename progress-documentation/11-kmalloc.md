# 11 — Kernel heap (kmalloc)

The PMM allocates whole 4 KiB frames. But most kernel data structures are much
smaller than that. We need something that can hand out arbitrary-sized chunks —
the kernel's version of `malloc`.

## Bump allocator

We went with the simplest possible approach: a **bump allocator**. It maintains a
single pointer (`heap_ptr`) that starts right after the end of the kernel image and
only ever moves forward.

`kmalloc(size)`:
1. Round `heap_ptr` up to the nearest 8-byte boundary (avoids alignment faults)
2. Record the current `heap_ptr` as the address to return
3. Add `size` to `heap_ptr`
4. Return the recorded address

That's the entire implementation. No headers, no free lists. `kfree()` is a no-op —
freed memory just stays leaked. This is fine for now because Phase 2 only allocates
a handful of things at boot time and never frees them.

`kmalloc_aligned(size, align)` is the same but rounds to an arbitrary alignment
before bumping. Page tables need to be 4 KiB aligned, so that variant exists.

## Why not something fancier

A free-list allocator would let us reuse freed memory, but it's ~100 lines vs.
~15 lines for the bump allocator, and we don't actually need reuse yet. Phase 3
(task stacks) will need more allocations, but the bump allocator still handles
that fine since stacks never get freed during a kernel's lifetime.

## Smoke test

```
KMALLOC_OK
```

Two allocations of 16 bytes each. We check they're non-null, non-overlapping, and
that writes to them survive. This also verifies that paging is covering the heap
region (the kernel image end is within our 16 MiB identity map).
