# Phase 3 Handoff — Multithreading, Scheduling, Multiprocessing

## Who you are

You are continuing work on a custom x86 32-bit OS for BSIT 338 (undergrad final
project). This is a group project; Phase 3 is attributed to **Bishesh**.

**Git identity for every commit:**
```
name:  bisheshr
email: braut429@gmail.com
```

Use this for every commit:
```bash
git -c user.name="bisheshr" -c user.email="braut429@gmail.com" commit -m "..."
```

**Do NOT add `Co-Authored-By` or any Claude attribution to any commit message.**

---

## Branch workflow

1. Pull latest `main` first: `git pull origin main`
2. Create a new branch: `git checkout -b phase3`
3. Make small, focused commits (one feature at a time)
4. When a logical section is done: `git push -u origin phase3`
5. Do NOT merge into main yourself — the user will open a PR and merge

---

## Current codebase state (end of Phase 2)

All code is at `/Users/aarohan/os/project/`. Build with `make`, test with
`make test`. The test harness runs QEMU headless and greps COM1 serial output.

**Serial output at end of Phase 2:**
```
BOOT_OK → VGA_OK → GDT_OK → IDT_OK → ISR3_OK → PIC_OK →
PIT_OK → TIMER_OK → KBD_OK → MOUSE_OK →
PMM: 32615 free frames (130460 KiB) → PMM_OK →
PAGING_OK → HEAP_OK → KMALLOC_OK
```

**Directory layout:**
```
boot/           multiboot stub (multiboot.asm)
kernel/         GDT, IDT, ISR stubs, PIC, PIT, keyboard, mouse, VGA, serial
mm/             PMM (bitmap), paging (identity map 0–16 MiB), kmalloc (bump)
include/        types.h (stdint/stddef/stdbool), io.h (inb/outb)
scripts/        test.sh — headless QEMU test harness
linker.ld       exports _kernel_end; kernel loads at 1 MiB
Makefile        CC=i686-elf-gcc, EXPECT=KMALLOC_OK
```

**Key APIs already available:**
- `pmm_alloc_frame()` / `pmm_free_frame(addr)` — physical page allocator
- `kmalloc(size)` / `kmalloc_aligned(size, align)` — kernel bump heap
- `pit_get_ticks()` — current tick count at 100 Hz
- `irq_install_handler(irq, fn)` — install an IRQ handler
- `serial_write(s)` / `serial_write_uint(n)` — COM1 logging
- `vga_write(s)` / `vga_set_color(fg, bg)` — VGA text output

---

## Phase 3 — what to build (3 items, roadmap items 9–11)

### Item 9 — Multithreading (`sched/`)

Kernel threads: each thread has its own stack and a saved CPU context. A
hand-written `context_switch` in asm saves/restores EIP, ESP, EBP, EBX, ESI,
EDI (callee-saved regs) and switches stacks.

**Task control block (TCB):**
```c
typedef struct task {
    uint32_t esp;          // saved stack pointer (all other regs pushed on stack)
    uint32_t *stack;       // base of allocated stack (for cleanup)
    uint32_t stack_size;
    void (*entry)(void);   // entry function
    int      state;        // RUNNING, READY, DEAD
    struct task *next;     // scheduler linked list
} task_t;
```

**context_switch (asm):**
```asm
; void context_switch(uint32_t *old_esp, uint32_t new_esp);
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]   ; old_esp arg
    mov [eax], esp         ; save current ESP
    mov esp, [esp + 24]   ; load new ESP (after the push, offsets shift)
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                    ; jumps to new task's saved EIP
```

Stack size per thread: allocate 8 KiB from kmalloc. Initial stack frame for a
new thread should look like it just called `context_switch` — push entry address
and four callee-saved zeros, then set `task->esp` to the top of that frame.

### Item 10 — CPU Scheduling (`sched/`)

**Round-robin preemptive scheduler driven by the PIT tick (IRQ0).**

The existing PIT handler (`pit_tick` in `pit.c`) calls `pit_get_ticks()`. Modify
or extend it to call `schedule()` every N ticks (e.g., every 10 ticks = 100 ms
time slice). `schedule()` picks the next READY task from the circular list and
calls `context_switch`.

```c
void schedule(registers_t *r) {
    (void)r;
    task_t *next = find_next_ready(current_task->next);
    if (!next || next == current_task) return;
    task_t *prev = current_task;
    current_task = next;
    context_switch(&prev->esp, next->esp);
}
```

Create a `sched_init()` that sets up the idle task (the current kernel thread,
which is already running) as task 0, then additional tasks can be added with
`task_create(entry_fn)`.

### Item 11 — Multiprocessing (`sched/`)

Each "process" gets its **own page directory (CR3)**. On context switch, swap
CR3 when moving between processes. For this project, "processes" are
compiled-in C functions — no ELF loading needed.

Extend the TCB:
```c
typedef struct task {
    ...
    uint32_t *page_dir;    // physical address of this task's page directory
    uint32_t  cr3;         // same value, kept for clarity
} task_t;
```

`context_switch` extension: after switching ESP, if `new_cr3 != old_cr3`, write
it to CR3:
```asm
mov eax, [new_cr3_ptr]
mov cr3, eax           ; flushes TLB
```

Each process can share the kernel's identity-mapped page directory initially
(just copy the 4 entries for 0–16 MiB), then get private upper regions.

---

## Suggested commit sequence (small commits)

```
feat(sched): add task_t TCB struct and task_create()
feat(sched): hand-written context_switch in asm
feat(sched): round-robin scheduler hooked into PIT IRQ0
test(sched): two kernel threads printing alternating messages
feat(sched): per-process page directory (CR3 switch on context_switch)
test(sched): two processes with separate address spaces
docs: progress 12 (multithreading)
docs: progress 13 (CPU scheduling)
docs: progress 14 (multiprocessing)
```

---

## Testing strategy

After each item, add a marker to `kernel_main` and update `EXPECT` in Makefile:

| Item | Marker |
|------|--------|
| Multithreading | `THREADS_OK` |
| Scheduling | `SCHED_OK` |
| Multiprocessing | `MP_OK` |

For threading/scheduling tests: create two tasks that each increment a counter
or write to serial. Spin in kernel_main for N ticks, then check both counters
advanced — proving both tasks actually ran.

---

## Architecture constraints (do not change)

- x86 32-bit protected mode, ring 0 only
- No user mode, no ELF loading, no syscalls
- Single CPU (no SMP)
- Toolchain: `i686-elf-gcc`, `nasm -f elf32`
- Boot: GRUB2 Multiboot1
- Test runner: `./scripts/test.sh <MARKER>`

---

## Where things are

- This file: `docs/phase3-handoff.md`
- Context/goals: `context.md` (project overview and roadmap)
- Phase 1 progress docs: `progress-documentation/00` through `06`
- Phase 2 progress docs: `progress-documentation/07` through `11`
- Phase 3 progress docs to write: `12`, `13`, `14`
