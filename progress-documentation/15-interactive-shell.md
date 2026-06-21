# 15 — Interactive Shell

Up to now the kernel ran a fixed script: boot, run every self-test, print markers
to the serial port, then halt. Nothing a person sat in front of could *do*. This
step turns omen into something you can actually drive — a keyboard-driven shell
with a prompt, line editing, and commands that run each of the six features on
demand.

## The shape of a shell

A shell is just a loop — the classic **REPL**: Read a line, Evaluate it, Print
the result, Loop. Ours lives in its own module, `shell/shell.c`, and exposes a
single entry point:

```c
void shell_run(void);   // never returns
```

`kernel_main` calls it once the boot self-tests have passed. We didn't want to
lose the headless test harness, though — that's what proves the kernel still
works without a human. So the hand-off is gated at compile time:

```c
#ifdef RUN_TESTS_ONLY
    qemu_exit(0);                 // the test build exits so the grep harness can run
#else
    shell_run();                  // the normal build drops you at the prompt
#endif
```

`make test` builds a second kernel (`kernel-test.bin`) with `-DRUN_TESTS_ONLY`
so the automated marker-grep still works, while the default `make` build is the
interactive one.

## Reading a line

The REPL reads one key at a time from the keyboard driver built back in step 7.
`keyboard_getc()` returns the next ASCII character, or `0` if nothing is waiting.
When nothing is waiting we `hlt` — the CPU sleeps until the next interrupt
instead of spinning hot.

Three keys are special:

- **Enter** (`\n`) — terminate the line, run it, print a fresh prompt.
- **Backspace** (`\b`) — step back one character, both in our buffer and on
  screen. (We taught `vga_putc` to handle `\b` destructively, even stepping back
  across a wrapped line.)
- everything else — append to the line buffer and echo it so you can see what
  you typed.

```c
char c = keyboard_getc();
if (c == 0)        { __asm__ volatile ("hlt"); continue; }
if (c == '\n')     { run_line(line); ... }
else if (c == '\b'){ if (len) { len--; vga_putc('\b'); } }
else if (len < LINE_MAX - 1) { line[len++] = c; vga_putc(c); }
```

## Parsing and dispatch

Once a line is entered we tokenize it in place — every space becomes a `\0`, and
`argv[]` points at the start of each word. The first token is the command name.
We look it up in a static table:

```c
typedef struct { const char *name; cmd_fn fn; const char *help; } command_t;
```

Each entry is a name, a function pointer, and a one-line help string. `help`
walks this same table to print itself, so the command list can never drift out
of sync with the documentation. Unknown commands get a friendly "try 'help'".

There is no libc in a freestanding kernel, so even `strcmp` is ours — a six-line
`shell_strcmp` written by hand.

## The commands

**Informational:**

| command | what it does |
|---------|--------------|
| `help`    | list every command and its description |
| `about`   | OS name and the feature checklist |
| `clear`   | blank the screen |
| `echo`    | print its arguments back |
| `meminfo` | free physical frames, via the PMM's `pmm_free_count()` |
| `uptime`  | ticks since boot ÷ 100 Hz, via the PIT's `pit_get_ticks()` |

**Live feature demos** — these are the heart of the shell. Each one spawns fresh
tasks on every invocation, reusing the exact primitives the boot self-tests use:

- `threads` — spawns two kernel threads that print `A` and `B` interleaved,
  yielding between each character. Proves the context switch from step 12.
- `sched`   — spawns two workers that *never* yield, lets them run under the PIT
  preemption timer for 30 ticks, then reports both counters are non-zero. Proves
  the preemptive round-robin scheduler from step 13.
- `proc`    — spawns two processes that map the *same* virtual address into
  *different* physical frames and confirms neither sees the other's value.
  Proves the address-space isolation from step 14.
- `mouse`   — enters a live tracking mode: an `X` follows the mouse around the
  screen with a button-state readout on the bottom row, until you press a key.
  Proves the mouse driver from step 8.
- `reboot`  — cleanly exits QEMU.

The demos block until they finish (`while (!done) sched_yield();`) and then
return to the prompt, so the shell stays responsive.

## Proof

The shell prints a `SHELL_READY` marker to serial the moment the prompt comes up,
so the harness can confirm the boot path reaches the shell:

```
SHELL_READY
```

Interactive input was verified headlessly by driving QEMU's emulated PS/2
keyboard through the monitor: typing `help`, `about`, `meminfo`, `uptime`,
`threads`, `sched`, `proc` and finally `reboot` ran every command path without a
crash and exited cleanly with:

```
SHELL_EXIT
```

That closes the loop — boot, drivers, threads, scheduling, processes, and now a
shell that lets a person exercise all of them by hand.
