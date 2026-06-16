# 14 — Multiprocessing: isolated address spaces

**What we built:** the leap from *threads* to *processes*. Every process gets its
**own page directory**, and the context switch now swaps `CR3` along with the
stack. Two processes can use the **same virtual address** for completely different
memory — neither can see or corrupt the other's data.

## The big idea: threads share memory, processes don't

The threads from steps 12–13 all share one address space — the kernel's
identity-mapped page directory. That's fine for kernel tasks, but it means any
thread can read or clobber any other thread's memory. A real **process** is
isolated: it has a private view of memory, enforced by the hardware.

On x86 that private view is a **page directory**, and the CPU finds the active one
through the `CR3` register. Switch `CR3` and you switch address spaces instantly —
the same virtual address now resolves through a different set of page tables to a
different physical frame. Isolation is therefore just: *give each process its own
page directory, and load it on every context switch.*

## Extending the context switch

`CR3` is loaded right after the stack swap. Writing `CR3` flushes the TLB (the
CPU's cache of address translations), so stale mappings from the previous process
can't leak through.

```asm
    mov esp, edx           ; switch onto the new stack
    mov ecx, [esp + 28]    ; incoming CR3
    test ecx, ecx
    jz .skip_cr3           ; cr3 == 0 → leave the address space alone
    mov cr3, ecx           ; load new page directory (also flushes the TLB)
.skip_cr3:
```

Every task carries a `cr3` field. Plain kernel threads set it to the kernel's own
directory, so reloading it is harmless (same mapping). A process sets it to a
private directory built by `process_create_space`.

## Building a private address space

A process can't throw away the kernel's mappings — its own code, stack, and the
interrupt handlers all live in the kernel's identity-mapped low 16 MiB. So a new
directory **copies the kernel's low mappings** and then adds one private page:

```c
uint32_t process_create_space(uint32_t vaddr) {
    uint32_t *dir  = kmalloc_aligned(4096, 4096);   // new page directory
    uint32_t *kdir = (uint32_t *)paging_kernel_dir();

    for (int i = 0; i < 1024; i++) dir[i] = 0;
    for (int i = 0; i < 4; i++)    dir[i] = kdir[i]; // share kernel 0–16 MiB

    uint32_t *pt    = kmalloc_aligned(4096, 4096);   // new page table
    uint32_t  frame = pmm_alloc_frame();             // a private physical frame
    pt[(vaddr >> 12) & 0x3FF] = frame | PAGE_PRESENT | PAGE_WRITE;
    dir[vaddr >> 22]          = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITE;

    return (uint32_t)dir;   // identity-mapped, so virtual == physical == CR3
}
```

Two processes calling this with the **same** `vaddr` get two **different** physical
frames behind that address — the essence of isolation.

## Proving it works

Two processes both map `0x01000000` (16 MiB — deliberately *unmapped* in the
kernel directory, so it only exists inside a process). Each writes its own
signature, lets the other run via `sched_yield`, then reads the value back:

```c
static void proc_a(void) {
    volatile uint32_t *p = (uint32_t *)0x01000000;
    *p = 0xAAAAAAAA;
    for (int i = 0; i < 4; i++) sched_yield();
    mp_a = (*p == 0xAAAAAAAA);   // still our value?
}
// proc_b is identical with 0xBBBBBBBB
```

If the address spaces were shared, whoever wrote last would win and the other
would read the wrong signature. Instead both read back their own value — proving
the `0x01000000` in `proc_a` and the `0x01000000` in `proc_b` are genuinely
different memory. We print `MP_OK`.

```
THREADS_OK
SCHED_OK
MP_OK
```

## Where we are now

- ✅ `CR3` swapped on every context switch (TLB flushed automatically).
- ✅ `process_create_space` builds a private directory sharing the kernel's low map.
- ✅ Two processes share a virtual address but not the physical memory — `MP_OK`.
- ✅ Phase 3 complete: **multithreading, CPU scheduling, multiprocessing**.

**Jargon recap:** *process* = a thread with its own isolated address space.
*page directory* = the top-level table the CPU uses to translate virtual to
physical addresses. *CR3* = the register holding the physical address of the
active page directory; changing it switches address spaces. *TLB* = Translation
Lookaside Buffer, the CPU's cache of recent address translations, flushed when
CR3 is written. *address-space isolation* = the guarantee that one process cannot
read or write another's memory.
