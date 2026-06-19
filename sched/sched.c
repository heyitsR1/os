#include "sched.h"
#include "../mm/kmalloc.h"
#include "../mm/paging.h"

#define STACK_SIZE 8192   // 8 KiB kernel stack per thread

// Defined in context_switch.asm.
extern void context_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);
extern void thread_trampoline(void);

static task_t *current = 0;   // the task currently on the CPU

task_t *sched_current(void) { return current; }

// Where a thread lands when its entry function returns. It will never be
// scheduled again, so just mark it dead and yield forever.
static void thread_exit(void) {
    current->state = TASK_DEAD;
    for (;;) sched_yield();
}

void sched_init(void) {
    // The kernel is already running on its boot stack; adopt that as task 0.
    // Its ESP gets filled in the first time we switch away from it.
    task_t *t = (task_t *)kmalloc(sizeof(task_t));
    t->esp = 0;
    t->stack = 0;
    t->stack_size = 0;
    t->entry = 0;
    t->state = TASK_RUNNING;
    t->cr3 = paging_kernel_dir();   // task 0 runs in the kernel address space
    t->next = t;              // a ring of one
    current = t;
}

task_t *task_create(void (*entry)(void)) {
    task_t   *t     = (task_t *)kmalloc(sizeof(task_t));
    uint32_t *stack = (uint32_t *)kmalloc(STACK_SIZE);

    t->stack      = stack;
    t->stack_size = STACK_SIZE;
    t->entry      = entry;
    t->state      = TASK_READY;
    t->cr3        = paging_kernel_dir();  // share kernel space until made a process

    // Forge an initial stack so the first context_switch into this task
    // returns into thread_trampoline (sti) -> entry. When entry returns it
    // falls through to thread_exit. Stack grows down, so push high-to-low:
    uint32_t *sp = (uint32_t *)((uint8_t *)stack + STACK_SIZE);
    *(--sp) = (uint32_t)thread_exit;        // entry's return address
    *(--sp) = (uint32_t)entry;              // thread_trampoline 'ret' target
    *(--sp) = (uint32_t)thread_trampoline;  // context_switch 'ret' target
    *(--sp) = 0;   // ebp  } callee-saved registers restored by context_switch
    *(--sp) = 0;   // ebx
    *(--sp) = 0;   // esi
    *(--sp) = 0;   // edi
    t->esp = (uint32_t)sp;

    // Splice into the circular list immediately after the current task.
    t->next       = current->next;
    current->next = t;
    return t;
}

// Walk the ring from `start` and return the first non-dead task.
static task_t *find_next_ready(task_t *start) {
    task_t *t = start;
    do {
        if (t->state != TASK_DEAD) return t;
        t = t->next;
    } while (t != start);
    return start;
}

void schedule(void) {
    if (!current) return;                       // scheduler not up yet
    task_t *next = find_next_ready(current->next);
    if (next == current) return;                // nothing else to run

    task_t *prev = current;
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;
    context_switch(&prev->esp, next->esp, next->cr3);
}

void sched_yield(void) {
    __asm__ volatile ("cli");
    schedule();
    __asm__ volatile ("sti");
}
