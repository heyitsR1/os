# 14 — Multiprocessing

Processes with isolated address spaces. Two processes can use the same virtual
address and get completely different memory — they can't see or corrupt each other.

## Threads vs processes

The threads from steps 12–13 all share one address space (the kernel's identity map).
Any thread can read or modify any other thread's memory. A **process** is isolated:
it has its own view of memory enforced by hardware.

On x86 that private view is a **page directory**. The CPU finds the active page
directory through the `CR3` register. To switch address spaces, you swap CR3.
That's it — the same virtual address now resolves through a different set of page
tables to a different physical frame.

## Extending the context switch

We added a CR3 swap right after the stack swap in `context_switch.asm`:

```asm
    mov esp, edx           ; switch stacks
    mov ecx, [esp + 28]    ; incoming CR3 value
    test ecx, ecx
    jz .skip_cr3           ; 0 means "don't change address space"
    mov cr3, ecx           ; load new page directory, flushes TLB
.skip_cr3:
```

Writing CR3 also flushes the TLB (the CPU's cache of address translations), so
there's no risk of stale mappings from the previous process leaking through.

## Building a private address space

A process can't throw away the kernel's mappings — its own code and stack live
there, and so do the interrupt handlers. So `process_create_space()` builds a new
page directory that **copies the kernel's first 4 entries** (covering 0–16 MiB)
and then adds one private page at the requested virtual address backed by a fresh
physical frame from the PMM.

Two processes calling it with the same virtual address get two different physical
frames behind that address — neither can see the other's memory.

## Proof

Two processes both map virtual address `0x01000000` (deliberately outside the
kernel's identity map, so it only exists inside a process). Each writes its own
signature value there, yields a few times to let the other run, then reads it back:

- Process A writes `0xAAAAAAAA`, yields, reads back `0xAAAAAAAA` ✓
- Process B writes `0xBBBBBBBB`, yields, reads back `0xBBBBBBBB` ✓

If the spaces were shared, whoever wrote last would win and the other would see the
wrong value. Both seeing their own value proves the isolation is real.

```
MP_OK
```

That's the end of Phase 3 and all 6 required OS features.
