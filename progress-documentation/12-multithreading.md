# 12 — Multithreading

Kernel threads — multiple execution contexts that can take turns on the CPU, each
with its own stack.

## What a thread actually is

On a single CPU, a thread is just a stack and a saved set of registers. At any
moment only one thread is running; the others are frozen with their registers
stored on their own stack. Switching threads means:

1. Push the current thread's callee-saved registers onto its stack
2. Save the current stack pointer into the thread's control block
3. Load the next thread's saved stack pointer
4. Pop the next thread's registers off its stack
5. `ret` — which pops the return address and jumps to wherever that thread left off

That's the whole mechanism. Everything else is bookkeeping.

## Task control block

Each thread has a `task_t`:

```c
typedef struct task {
    uint32_t      esp;         // saved stack pointer
    uint32_t     *stack;       // base of the 8 KiB stack
    uint32_t      stack_size;
    void        (*entry)(void);
    int           state;       // READY / RUNNING / DEAD
    uint32_t      cr3;         // page directory (used in step 14)
    struct task  *next;        // circular scheduler list
} task_t;
```

Tasks are linked in a circular list. The scheduler walks `current->next`, so
round-robin falls out naturally.

## The context switch

`sched/context_switch.asm` does the actual switch. The System V calling convention
says `EBX`, `ESI`, `EDI`, `EBP` are callee-saved — a function must preserve them.
We push exactly those four, save ESP, load the new ESP, pop four back, and `ret`:

```asm
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]    ; old_esp pointer argument
    mov [eax], esp         ; save current stack pointer
    mov esp, [esp + 24]    ; load new stack pointer
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                    ; jumps to the new thread's saved return address
```

## Starting a new thread

A thread that has never run has no saved context to resume. `task_create()` forges
one by writing a fake stack frame that looks exactly like the thread is frozen mid
context-switch. When the scheduler first switches into it, the `ret` pops an address
we planted — `thread_trampoline` — which runs `sti` (re-enables interrupts) then
jumps to the entry function. When the entry function returns it hits `thread_exit`,
which marks the task DEAD so the scheduler skips it.

## Proving it works

Two threads, five iterations each, alternating via `sched_yield()`. The serial output:

```
BABABABABA
THREADS_OK
```

Ten characters perfectly interleaved proves the CPU is genuinely switching between
two independent stacks and resuming exactly where each one paused.
