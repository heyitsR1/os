# 13 — CPU scheduling: preemptive round robin

**What we built:** a **preemptive round-robin scheduler**. Instead of waiting for
threads to politely call `sched_yield()`, the PIT now forces a switch every 10
ticks (100 ms). A thread that never yields — even an infinite loop — is still
taken off the CPU and another thread gets its turn.

## The big idea: taking control by force

Cooperative scheduling (step 12) has a fatal flaw: one badly-behaved thread that
loops forever hangs the whole machine, because nothing ever calls `schedule()`.
**Preemption** fixes this by using the one thing a thread cannot block: a hardware
interrupt. The PIT fires IRQ0 at 100 Hz no matter what code is running, so the
kernel always regains control on the next tick.

We already had the heartbeat (step 06). Scheduling is just: *on every Nth tick,
call `schedule()`.*

```c
#define PIT_SLICE 10            // 10 ticks @ 100 Hz = 100 ms time slice

static void pit_tick(registers_t *r) {
    (void)r;
    ticks++;
    if ((ticks % PIT_SLICE) == 0) schedule();
}
```

`schedule()` runs inside the interrupt handler, on the interrupted thread's stack.
When it calls `context_switch`, the *current* thread's context (including the
half-finished interrupt handler) is frozen, and a different thread resumes. The
frozen thread will pick up later — still inside the interrupt — and unwind back
to the code it was running. Nothing notices it was paused.

## The EOI problem (and the fix)

This is the subtle part. The PIC must receive an **End-Of-Interrupt** (EOI) before
it will deliver the next IRQ0. Normally the EOI is sent *after* the handler runs.
But our handler may `context_switch` into a **brand-new thread** that has never
run — and that thread jumps straight to its entry function. It never returns
through `irq_handler`, so the EOI for that tick would never be sent, and the timer
would go silent forever.

The fix is to acknowledge the PIC **before** dispatching the handler:

```c
void irq_handler(registers_t *r) {
    uint8_t irq = r->int_no - 32;
    pic_send_eoi(irq);                 // ack first...
    if (irq_handlers[irq]) irq_handlers[irq](r);   // ...then dispatch
}
```

Because the IRQ gate enters with interrupts disabled, there's no risk of the same
interrupt re-entering between the EOI and the handler. And when a previously
preempted thread later resumes inside `irq_handler`, the EOI line is already
behind it — so no interrupt is ever acknowledged twice.

## New threads must start with interrupts on

There's a matching subtlety on the thread side. A normal interrupt return (`iret`)
restores the interrupt flag, but a brand-new thread has no `iret` frame — it is
entered by a plain `ret` from `context_switch`. If we did nothing, that thread
would run with interrupts disabled and never be preempted. That's exactly why
`thread_trampoline` (step 12) runs `sti` before jumping to the entry function.

## Proving it works

Two **CPU-bound workers** that never call `sched_yield()` — they just spin
incrementing a counter:

```c
static void worker_p(void) { while (!stop_workers) work_p++; }
static void worker_q(void) { while (!stop_workers) work_q++; }
```

`kernel_main` spawns both, then waits ~30 ticks. Neither worker cooperates, so the
*only* way both counters can advance is if the timer is preempting them and
handing the CPU back and forth. Both advance, and we print `SCHED_OK`.

```
THREADS_OK
SCHED_OK
```

## Where we are now

- ✅ `pit_tick` calls `schedule()` every 10 ticks → 100 ms round-robin slices.
- ✅ EOI sent before dispatch, so switching into a fresh thread can't stall IRQ0.
- ✅ New threads start with interrupts enabled (`thread_trampoline` `sti`).
- ✅ Two non-yielding workers both make progress — `SCHED_OK`.
- ⏭️ Next: give each task its **own page directory** and swap CR3 on switch, so
  processes have isolated address spaces.

**Jargon recap:** *preemption* = forcibly switching tasks via a timer interrupt,
without the task's cooperation. *time slice* = how long a task runs before being
preempted (here 100 ms). *EOI* = End-Of-Interrupt, the signal the PIC needs before
sending the next interrupt. *round robin* = each ready task gets an equal turn in
a fixed cycle.
