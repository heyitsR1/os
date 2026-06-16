# 10 — Paging: virtual memory and address translation

**What we built:** an x86 32-bit **paging layer** that identity-maps the first
16 MiB of physical memory, loads the page directory into CR3, and sets the PG
bit in CR0 — enabling the MMU so that every future memory access goes through
hardware address translation.

## The big idea: why paging?

Without paging every process shares the same physical address space. One bug in
any task can overwrite any other task's memory or the kernel itself. Paging
interposes a hardware translation layer:

```
virtual address  →  [page directory + page table lookup]  →  physical address
```

Each process gets its own page directory (its own *view* of memory), making
address spaces independent. It is also the mechanism for memory protection: a
page can be marked not-present, read-only, or supervisor-only, so the CPU raises
a fault on illegal accesses.

For now we use a **single shared page directory** (all ring-0 kernel code) with
an identity mapping — virtual address N equals physical address N. This is the
simplest correct starting point and lets all existing pointers keep working after
paging is enabled.

## x86 two-level page table structure

With 32-bit addresses and 4 KiB pages:

```
31      22 21      12 11       0
[ PD idx ] [ PT idx ] [ offset ]
   10 bits    10 bits   12 bits
```

- **Page directory (PD):** 1024 entries × 4 bytes = 4 KiB. Entry `i` points to
  a page table that covers virtual addresses `[i × 4 MiB, (i+1) × 4 MiB)`.
- **Page table (PT):** 1024 entries × 4 bytes = 4 KiB. Entry `j` maps one
  4 KiB page.

## Our mapping

We statically allocate 4 page tables (covering 4 × 4 MiB = **16 MiB**) in
`.bss` with `__attribute__((aligned(4096)))`. Each PT entry is:

```
physical_address | PAGE_PRESENT | PAGE_WRITE
```

with `physical_address = (table_index * 1024 + entry_index) * 4096`.

This gives an exact identity map for `0x0000_0000`–`0x00FF_FFFF`. The kernel
sits at `0x0010_0000` (1 MiB) and the static page tables themselves are also
within this range, so after enabling paging everything continues to work.

## Enabling paging

```c
__asm__ volatile (
    "mov %0, %%cr3\n\t"          // load page directory base register
    "mov %%cr0, %%eax\n\t"
    "or  $0x80000000, %%eax\n\t" // set PG bit
    "mov %%eax, %%cr0\n\t"
    : : "r"(page_dir) : "eax"
);
```

The CPU immediately starts translating addresses through the new tables. Because
we identity-mapped, the instruction pointer (EIP) and stack pointer (ESP) remain
valid.

## Key files

| File | Role |
|------|------|
| `mm/paging.c` | static PD + 4 PTs, identity map 0–16 MiB, CR3/CR0 setup |
| `mm/paging.h` | `paging_init()`, `PAGE_PRESENT`, `PAGE_WRITE`, `PAGE_SIZE` |

## Serial output

```
PAGING_OK
```

Printed by `kernel_main` after `paging_init()` returns. The fact that execution
continues normally proves the identity mapping is correct.
