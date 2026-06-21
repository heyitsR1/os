#include "shell.h"
#include "../include/types.h"
#include "../kernel/vga.h"
#include "../kernel/serial.h"
#include "../kernel/keyboard.h"
#include "../kernel/pit.h"
#include "../mm/pmm.h"

#define LINE_MAX 128
#define MAX_ARGS 8
#define PROMPT   "omen> "

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

static const command_t commands[] = {
    { "help",    cmd_help,    "list available commands" },
    { "about",   cmd_about,   "show OS name and feature checklist" },
    { "clear",   cmd_clear,   "clear the screen" },
    { "echo",    cmd_echo,    "print the given text" },
    { "meminfo", cmd_meminfo, "show free physical memory" },
    { "uptime",  cmd_uptime,  "show time since boot" },
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
