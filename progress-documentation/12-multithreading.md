# 12 — Multithreading: kernel threads and the context switch

**What we built:** the core of a **kernel threading** system — a task control
block (TCB), a hand-written `context_switch` in assembly, and the ring of tasks
the scheduler walks. Two threads can now take turns running on a single CPU,
each on its own stack.

## The big idea: what a thread actually is

A "thread" sounds abstract, but on a single CPU it is concrete: a thread is just
**a stack plus a saved set of CPU registers**. At any instant only one thread is
truly running; the others are frozen, with everything they need to resume — their
stack and their register values — stored in memory. Switching threads means:

1. Save the running thread's registers onto its stack.
2. Remember where its stack pointer ended up.
3. Load the next thread's saved stack pointer.
4. Restore that thread's registers from its stack.
5. Jump to wherever it left off.

That is the entire trick. Everything else is bookkeeping.

## The task control block (TCB)

Each thread is described by one `task_t`. The most important field is `esp` — the
saved stack pointer. Because every other register is pushed *onto the stack*
during a switch, saving one pointer is enough to capture the whole frozen context.

```c
typedef struct task {
    uint32_t      esp;         // saved stack pointer (the whole context lives here)
    uint32_t     *stack;       // base of the 8 KiB stack (0 for task 0)
    uint32_t      stack_size;
    void        (*entry)(void);
    int           state;       // READY / RUNNING / DEAD
    uint32_t      cr3;         // page directory (used in step 14)
    struct task  *next;        // circular scheduler list
} task_t;
```

Tasks are linked into a **circular list**. The scheduler always just looks at
`current->next`, so round-robin falls out naturally — the last task points back
to the first.

## The context switch, by hand

The System V calling convention says four registers — `EBX`, `ESI`, `EDI`, `EBP` —
are *callee-saved*: a function must preserve them. We exploit this. `context_switch`
pushes exactly those four, saves `ESP`, loads the new `ESP`, pops the four back,
and `ret`s. The `ret` is the magic: it pops a return address off the *new* stack,
so the CPU jumps into the new thread.

```asm
; void context_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]    ; old_esp pointer
    mov [eax], esp         ; *old_esp = current ESP  (context fully saved)
    mov edx, [esp + 24]    ; incoming ESP
    mov esp, edx           ; switch stacks
    ; (CR3 handling added in step 14)
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                    ; jump to the new thread's saved EIP
```

## Bootstrapping a brand-new thread

A frozen thread resumes by `ret`-ing to a saved address — but a thread that has
*never run* has no saved context. So `task_create` **forges** one. It writes a
fake stack that looks exactly like a thread frozen mid-switch:

```
   higher addresses
   ┌──────────────────┐
   │ thread_exit       │ ← entry() returns here when it finishes
   │ entry             │ ← thread_trampoline 'ret' jumps here
   │ thread_trampoline │ ← context_switch 'ret' jumps here
   │ 0  (ebp)          │
   │ 0  (ebx)          │   restored by context_switch's four pops
   │ 0  (esi)          │
   │ 0  (edi)          │ ← task->esp points here
   └──────────────────┘
   lower addresses
```

The first switch into the thread pops the four zeros, then `ret`s into
`thread_trampoline`, a two-instruction stub that runs `sti` (enable interrupts)
and `ret`s again — landing in `entry`. When `entry` eventually returns, it falls
into `thread_exit`, which marks the task `DEAD` so the scheduler skips it forever.

## Cooperative scheduling first

Before wiring the timer, we proved the switch with **cooperative** multitasking:
`sched_yield()` disables interrupts, calls `schedule()`, and re-enables them.
`schedule()` finds the next non-dead task and switches to it.

```c
void schedule(void) {
    task_t *next = find_next_ready(current->next);
    if (next == current) return;          // nothing else to run
    task_t *prev = current;
    prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;
    context_switch(&prev->esp, next->esp, next->cr3);
}
```

## Proving it works

Two threads each loop five times, printing `A` / `B` and yielding after each
step. The serial line shows them strictly alternating, then `THREADS_OK`:

```
BABABABABA
THREADS_OK
```

Ten characters, perfectly interleaved, proves the CPU is genuinely hopping
between two independent stacks and resuming each one exactly where it paused.

## Where we are now

- ✅ `task_t` TCB and a circular task list.
- ✅ Hand-written `context_switch` saving/restoring callee-saved registers + ESP.
- ✅ `task_create` forges an initial stack so new threads start cleanly.
- ✅ Cooperative `sched_yield()` switches between two threads — `THREADS_OK`.
- ⏭️ Next: drive `schedule()` from the PIT so switching is **preemptive**.

**Jargon recap:** *thread* = a stack plus a saved register set; the unit the CPU
runs. *TCB* = task control block, the struct describing one thread. *context
switch* = saving one thread's registers and restoring another's. *callee-saved
registers* = EBX/ESI/EDI/EBP, which a function must preserve, so saving them is
enough to freeze a thread. *cooperative scheduling* = threads switch only when
they voluntarily call `yield`.
