# 10 — Paging

Paging enables virtual memory — the CPU translates every address through a table
before accessing physical RAM. This is what makes process isolation possible later:
each process gets its own table, so the same virtual address in two processes can
map to different physical memory.

## Why we need it

Without paging every process shares the same physical memory. One bad pointer in
any task can corrupt any other task, or the kernel itself. Paging interposes a
translation layer that the hardware enforces — a page can be marked not-present or
read-only, and the CPU faults on violations.

## How x86 paging works

With 32-bit addresses and 4 KiB pages, every virtual address is split into three
parts:

```
bits 31-22: page directory index (which page table)
bits 21-12: page table index (which 4 KiB page)
bits 11-0:  offset within that page
```

The CPU starts from a **page directory** whose physical address is in the `CR3`
register. Each directory entry points to a **page table**. Each page table entry
maps one 4 KiB page to a physical frame.

## Our setup

For now we use a single page directory (the kernel's). We statically allocate 4
page tables covering the first 16 MiB of physical memory and identity-map them —
virtual address = physical address for everything in that range. This means all our
existing pointers still work after paging is turned on, since the addresses don't
change.

Enabling paging: write the page directory's physical address into `CR3`, then set
the PG bit in `CR0`. The CPU starts translating every memory access immediately.
Because we identity-mapped, the EIP (instruction pointer) and ESP (stack pointer)
remain valid through that transition.

## Testing

`PAGING_OK` is printed after `paging_init()` returns. If the mapping were wrong
the kernel would page-fault and crash before getting there.
