# 11 — Kernel heap: bump allocator (kmalloc)

**What we built:** a minimal **kernel heap allocator** that provides
`kmalloc()` / `kmalloc_aligned()` / `kfree()` for dynamic memory allocation
inside the kernel. The current implementation is a **bump allocator** — fast,
simple, and sufficient for Phase 2.

## The big idea: why the kernel needs a heap

The PMM hands out whole 4 KiB frames. But kernel data structures (task control
blocks, page tables, buffers) are rarely exactly 4 KiB. We need a finer-grained
allocator that carves up memory into arbitrary-sized chunks — the kernel
equivalent of `malloc`.

## Bump allocator: one pointer, always forward

A bump allocator maintains a single pointer, `heap_ptr`, that starts at the
first free byte after the kernel image and only ever moves forward:

```
[ kernel image ] [ free... ] ← heap_ptr grows →
                ↑
           heap_base (_kernel_end, page-aligned)
```

`kmalloc(size)`:
1. Align `heap_ptr` up to 8 bytes (prevents misaligned access faults).
2. Record current `heap_ptr` as the allocation address.
3. Add `size` to `heap_ptr`.
4. Return the recorded address.

That is all. No headers, no free lists, no fragmentation tracking.

## Why a bump allocator for now

| Property | Bump allocator | Free-list allocator |
|----------|---------------|---------------------|
| Alloc cost | O(1) | O(n) first-fit |
| Free cost | O(1) no-op | O(1)–O(n) |
| Fragmentation | zero (no reuse) | depends on policy |
| Code size | ~15 lines | ~100+ lines |
| Needed for Phase 2 | yes | no |

Phase 2 only allocates small objects at boot time (task stacks come in Phase 3).
A bump allocator gives us `kmalloc` immediately without the complexity of a real
allocator. `kfree()` is a deliberate no-op — freed memory is simply leaked until
Phase 3 upgrades this to a proper free-list.

## `kmalloc_aligned`

Some allocations require stricter alignment than 8 bytes (e.g., page tables must
be 4 KiB aligned). `kmalloc_aligned(size, align)` rounds `heap_ptr` up to the
nearest multiple of `align` before bumping:

```c
heap_ptr = (heap_ptr + align - 1) & ~(align - 1);
```

`kmalloc(size)` is just `kmalloc_aligned(size, 8)`.

## Smoke test

```c
uint32_t *a = kmalloc(16);   *a = 0xDEADBEEF;
uint32_t *b = kmalloc(16);   *b = 0xCAFEBABE;
// verify a != b and values survive
```

```
KMALLOC_OK
```

Both blocks are non-null, non-overlapping, and hold their written values after
paging is enabled — confirming that the identity map covers the heap region and
writes reach physical DRAM.

## Key files

| File | Role |
|------|------|
| `mm/kmalloc.c` | bump pointer, `kmalloc`, `kmalloc_aligned`, `kfree` stub |
| `mm/kmalloc.h` | public API |
| `linker.ld` | `_kernel_end` symbol used as heap base |
