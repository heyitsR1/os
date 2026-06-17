# 13 — CPU scheduling

Preemptive round-robin scheduler — threads get switched out by the timer even if
they never voluntarily yield.

## The problem with cooperative scheduling

Step 12 had cooperative multithreading: threads switch only when they call
`sched_yield()`. The obvious flaw: if any thread loops forever without yielding,
the whole machine is stuck. You can't fix a looping thread from inside it.

The solution is hardware preemption. The PIT fires IRQ0 at 100 Hz regardless of
what code is running. If the IRQ handler calls the scheduler, we can switch tasks
whether the current one cooperates or not.

We already had the timer from step 6. Adding preemption was one line in `pit_tick`:

```c
if ((ticks % PIT_SLICE) == 0) schedule();
```

Every 10 ticks (100 ms), force a switch.

## The EOI ordering problem

There's a subtle issue. The PIC needs an End-Of-Interrupt signal before it will
deliver the next IRQ0. Normally we send EOI after the handler runs. But if the
handler switches into a brand-new thread, that thread jumps straight to its entry
function and never returns through the normal handler path — so EOI would never be
sent, and the timer would go silent permanently.

The fix: send EOI *before* dispatching the handler. Because the IRQ gate disables
interrupts on entry, there's no risk of re-entrancy between the EOI and the handler
call.

```c
void irq_handler(registers_t *r) {
    uint8_t irq = r->int_no - 32;
    pic_send_eoi(irq);                          // acknowledge first
    if (irq_handlers[irq]) irq_handlers[irq](r); // then dispatch
}
```

## New threads need interrupts enabled

A thread resumed by a normal interrupt return (`iret`) gets its interrupt flag
restored from the saved flags on the stack. But a brand-new thread has no `iret`
frame — it's entered via a plain `ret`. If we did nothing it would run with
interrupts disabled and never get preempted. That's why `thread_trampoline` runs
`sti` before jumping to the thread's entry function.

## Proof

Two worker threads that never call `sched_yield()`, just spin incrementing counters.
After 30 timer ticks, both counters have to show progress — that's only possible if
the timer forcibly switched between them:

```
SCHED_OK
```
