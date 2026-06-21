#include "shell.h"
#include "../include/types.h"
#include "../kernel/vga.h"
#include "../kernel/serial.h"
#include "../kernel/keyboard.h"
#include "../kernel/pit.h"
#include "../mm/pmm.h"
#include "../sched/sched.h"
#include "../sched/proc.h"
#include "../kernel/mouse.h"
#include "../include/io.h"

#define LINE_MAX 128
#define MAX_ARGS 8
#define PROMPT   "omen> "

// --- threads demo ---
static volatile int td_a = 0, td_b = 0;
static void td_thread_a(void) {
    for (int i = 0; i < 5; i++) { td_a++; vga_putc('A'); sched_yield(); }
}
static void td_thread_b(void) {
    for (int i = 0; i < 5; i++) { td_b++; vga_putc('B'); sched_yield(); }
}

// --- scheduling demo ---
static volatile uint32_t sd_p = 0, sd_q = 0;
static volatile int sd_stop = 0;
static void sd_worker_p(void) { while (!sd_stop) sd_p++; }
static void sd_worker_q(void) { while (!sd_stop) sd_q++; }

// --- process isolation demo ---
#define SHELL_MP_VADDR 0x01000000u
static volatile int pd_a = 0, pd_b = 0;
static void pd_proc_a(void) {
    volatile uint32_t *p = (volatile uint32_t *)SHELL_MP_VADDR;
    *p = 0xAAAAAAAAu;
    for (int i = 0; i < 4; i++) sched_yield();
    pd_a = (*p == 0xAAAAAAAAu);
}
static void pd_proc_b(void) {
    volatile uint32_t *p = (volatile uint32_t *)SHELL_MP_VADDR;
    *p = 0xBBBBBBBBu;
    for (int i = 0; i < 4; i++) sched_yield();
    pd_b = (*p == 0xBBBBBBBBu);
}

// --- tiny freestanding string helpers ---
static int shell_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

// Split `line` in place into argv tokens on spaces. Returns argc.
static int shell_tokenize(char *line, char *argv[]) {
    int argc = 0;
    int in_tok = 0;
    for (char *p = line; *p; p++) {
        if (*p == ' ') { *p = 0; in_tok = 0; }
        else if (!in_tok) {
            if (argc < MAX_ARGS) argv[argc++] = p;
            in_tok = 1;
        }
    }
    return argc;
}

// --- command table ---
typedef void (*cmd_fn)(int argc, char *argv[]);
typedef struct { const char *name; cmd_fn fn; const char *help; } command_t;

static void cmd_help(int argc, char *argv[]);
static void cmd_about(int argc, char *argv[]);
static void cmd_clear(int argc, char *argv[]);
static void cmd_echo(int argc, char *argv[]);
static void cmd_meminfo(int argc, char *argv[]);
static void cmd_uptime(int argc, char *argv[]);
static void cmd_threads(int argc, char *argv[]);
static void cmd_sched(int argc, char *argv[]);
static void cmd_proc(int argc, char *argv[]);
static void cmd_mouse(int argc, char *argv[]);
static void cmd_reboot(int argc, char *argv[]);

static const command_t commands[] = {
    { "help",    cmd_help,    "list available commands" },
    { "about",   cmd_about,   "show OS name and feature checklist" },
    { "clear",   cmd_clear,   "clear the screen" },
    { "echo",    cmd_echo,    "print the given text" },
    { "meminfo", cmd_meminfo, "show free physical memory" },
    { "uptime",  cmd_uptime,  "show time since boot" },
    { "threads", cmd_threads, "demo multithreading (interleaved threads)" },
    { "sched",   cmd_sched,   "demo preemptive CPU scheduling" },
    { "proc",    cmd_proc,    "demo multiprocessing (isolated memory)" },
    { "mouse",   cmd_mouse,   "live mouse tracking (any key to exit)" },
    { "reboot",  cmd_reboot,  "quit QEMU" },
};
static const int n_commands = (int)(sizeof(commands) / sizeof(commands[0]));

static void cmd_help(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_write("available commands:\n");
    for (int i = 0; i < n_commands; i++) {
        vga_write("  ");
        vga_write(commands[i].name);
        vga_write("  - ");
        vga_write(commands[i].help);
        vga_putc('\n');
    }
}

static void cmd_about(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_write("omen OS\n");
    vga_write("  boot: multiboot magic verified at startup\n");
    vga_write("  subsystems: GDT, IDT, PIC, PIT(100Hz), paging, heap\n");
    vga_write("  features: keyboard, mouse, threads, scheduling, processes\n");
}

static void cmd_clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        vga_write(argv[i]);
        if (i < argc - 1) vga_putc(' ');
    }
    vga_putc('\n');
}

static void cmd_meminfo(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t frames = pmm_free_count();
    vga_write("free physical frames: ");
    vga_write_uint(frames);
    vga_write(" (");
    vga_write_uint(frames * 4);
    vga_write(" KiB)\n");
    vga_write("heap: bump allocator (kfree is a no-op)\n");
}

static void cmd_uptime(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t ticks = pit_get_ticks();
    vga_write("uptime: ");
    vga_write_uint(ticks);
    vga_write(" ticks (");
    vga_write_uint(ticks / 100);
    vga_write(" s at 100 Hz)\n");
}

static void cmd_threads(int argc, char *argv[]) {
    (void)argc; (void)argv;
    td_a = 0; td_b = 0;
    vga_write("spawning two threads (interleaved output): ");
    task_create(td_thread_a);
    task_create(td_thread_b);
    while (td_a < 5 || td_b < 5) sched_yield();
    vga_write("\nthreads ran concurrently -> context switch works\n");
}

static void cmd_sched(int argc, char *argv[]) {
    (void)argc; (void)argv;
    sd_p = 0; sd_q = 0; sd_stop = 0;
    vga_write("two non-yielding workers under preemption...\n");
    task_create(sd_worker_p);
    task_create(sd_worker_q);
    uint32_t t0 = pit_get_ticks();
    while (pit_get_ticks() - t0 < 30) __asm__ volatile ("hlt");
    sd_stop = 1;
    for (int i = 0; i < 4; i++) sched_yield();
    vga_write("worker P count: "); vga_write_uint(sd_p); vga_putc('\n');
    vga_write("worker Q count: "); vga_write_uint(sd_q); vga_putc('\n');
    if (sd_p > 0 && sd_q > 0)
        vga_write("both advanced without yielding -> preemptive RR works\n");
    else
        vga_write("a worker never ran -> preemption FAILED\n");
}

static void cmd_proc(int argc, char *argv[]) {
    (void)argc; (void)argv;
    pd_a = 0; pd_b = 0;
    vga_write("two processes, same vaddr, isolated address spaces...\n");
    task_t *pa = task_create(pd_proc_a);
    pa->cr3 = process_create_space(SHELL_MP_VADDR);
    task_t *pb = task_create(pd_proc_b);
    pb->cr3 = process_create_space(SHELL_MP_VADDR);
    while (!pd_a || !pd_b) sched_yield();
    if (pd_a && pd_b)
        vga_write("each kept its own value -> address spaces isolated\n");
    else
        vga_write("a process saw the other's value -> isolation FAILED\n");
}

static void cmd_mouse(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_write("move the mouse; press any key to exit.\n");
    uint8_t last_x = 0, last_y = 0;
    char prev = ' ';
    for (;;) {
        if (keyboard_getc() != 0) break;   // any key exits

        mouse_state_t m = mouse_get_state();
        uint8_t x = (uint8_t)m.x, y = (uint8_t)m.y;

        // restore the cell the cursor was on, then draw at the new spot
        vga_put_at(last_x, last_y, prev);
        prev = 'X';
        vga_put_at(x, y, 'X');
        last_x = x; last_y = y;

        // status line on row 24: a label plus L/R/M button indicators
        for (uint8_t i = 0; i < 50; i++) vga_put_at(i, 24, ' ');
        const char *label = "cursor=X  buttons: ";
        uint8_t col = 0;
        for (const char *s = label; *s; s++) vga_put_at(col++, 24, *s);
        uint8_t b = m.buttons;
        vga_put_at(col++, 24, (b & 1) ? 'L' : '-');
        vga_put_at(col++, 24, (b & 2) ? 'R' : '-');
        vga_put_at(col++, 24, (b & 4) ? 'M' : '-');

        for (volatile int d = 0; d < 200000; d++) { }   // small debounce delay
    }
    vga_put_at(last_x, last_y, prev == 'X' ? ' ' : prev);  // clean cursor
    vga_write("\nexited mouse mode.\n");
}

static void cmd_reboot(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_write("bye.\n");
    serial_write("SHELL_EXIT\n");
    outb(0xF4, 0x00);   // QEMU isa-debug-exit
    for (;;) __asm__ volatile ("hlt");
}

// Parse and run one entered line.
static void run_line(char *line) {
    char *argv[MAX_ARGS];
    int argc = shell_tokenize(line, argv);
    if (argc == 0) return;                 // empty line
    for (int i = 0; i < n_commands; i++) {
        if (shell_strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    vga_write("unknown command: ");
    vga_write(argv[0]);
    vga_write("  (try 'help')\n");
}

void shell_run(void) {
    char line[LINE_MAX];
    uint32_t len = 0;

    serial_write("SHELL_READY\n");   // marker so the harness can confirm launch
    vga_write("\n");
    vga_write(PROMPT);

    for (;;) {
        char c = keyboard_getc();
        if (c == 0) { __asm__ volatile ("hlt"); continue; }

        if (c == '\n') {
            vga_putc('\n');
            line[len] = 0;
            run_line(line);
            len = 0;
            vga_write(PROMPT);
        } else if (c == '\b') {
            if (len > 0) { len--; vga_putc('\b'); }
        } else if (len < LINE_MAX - 1) {
            line[len++] = c;
            vga_putc(c);
        }
    }
}
