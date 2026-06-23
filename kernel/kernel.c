#include "../include/types.h"
#include "../include/io.h"
#include "serial.h"
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "mouse.h"
#include "../mm/pmm.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../sched/sched.h"
#include "../sched/proc.h"
#include "../shell/shell.h"

static void __attribute__((unused)) qemu_exit(uint8_t code) {
    outb(0xF4, code);
}

// --- Phase 3, Item 9: cooperative multithreading test ---
// Two kernel threads that take turns via sched_yield(). The interleaved
// "ABAB..." on the serial line proves the context switch really alternates
// between two independent stacks.
static volatile int a_runs = 0, b_runs = 0;

static void thread_a(void) {
    for (int i = 0; i < 5; i++) { a_runs++; serial_write("A"); sched_yield(); }
}
static void thread_b(void) {
    for (int i = 0; i < 5; i++) { b_runs++; serial_write("B"); sched_yield(); }
}

// --- Phase 3, Item 10: preemptive scheduling test ---
// These workers never call sched_yield(); the only thing that can take the
// CPU away from them is the PIT preempting on IRQ0. If both counters advance
// while neither thread cooperates, the preemptive scheduler is working.
static volatile uint32_t work_p = 0, work_q = 0;
static volatile int stop_workers = 0;

static void worker_p(void) { while (!stop_workers) work_p++; }
static void worker_q(void) { while (!stop_workers) work_q++; }

// --- Phase 3, Item 11: multiprocessing test ---
// Two processes whose page directories both map MP_VADDR, but to different
// physical frames. Each writes its own signature there and, after letting the
// other run, reads it back. If both still see their own value, the address
// spaces are genuinely isolated — the CR3 swap on context switch works.
#define MP_VADDR 0x01000000u   // 16 MiB: unmapped in the kernel directory
static volatile int mp_a = 0, mp_b = 0;

static void proc_a(void) {
    volatile uint32_t *p = (volatile uint32_t *)MP_VADDR;
    *p = 0xAAAAAAAAu;
    for (int i = 0; i < 4; i++) sched_yield();
    mp_a = (*p == 0xAAAAAAAAu);
}
static void proc_b(void) {
    volatile uint32_t *p = (volatile uint32_t *)MP_VADDR;
    *p = 0xBBBBBBBBu;
    for (int i = 0; i < 4; i++) sched_yield();
    mp_b = (*p == 0xBBBBBBBBu);
}

// Print to both the on-screen terminal and the serial port.
static void klog(const char *s) {
    vga_write(s);
    serial_write(s);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    serial_init();
    serial_write("BOOT_OK\n");

    vga_init();
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    klog("omen OS - phase 1\n");
    klog("VGA_OK\n");

    gdt_init();
    klog("GDT_OK\n");

    idt_init();
    isr_install();
    klog("IDT_OK\n");

    __asm__ volatile ("int $0x3");   // breakpoint -> should print ISR3_OK

    pic_remap();
    irq_install();
    klog("PIC_OK\n");

    // Enable interrupts, then start the PIT at 100 Hz.
    __asm__ volatile ("sti");
    pit_init(100);
    klog("PIT_OK\n");

    // Spin for a few ticks to prove IRQ0 is actually firing.
    uint32_t start = pit_get_ticks();
    while (pit_get_ticks() - start < 3)
        __asm__ volatile ("hlt");
    klog("TIMER_OK\n");

    keyboard_init();
    klog("KBD_OK\n");

    mouse_init();
    klog("MOUSE_OK\n");

    pmm_init(mb_info);
    klog("PMM_OK\n");

    paging_init();
    klog("PAGING_OK\n");

    kmalloc_init();
    klog("HEAP_OK\n");

    // Smoke-test: two allocations must be non-null and non-overlapping.
    uint32_t *a = (uint32_t *)kmalloc(16);
    uint32_t *b = (uint32_t *)kmalloc(16);
    *a = 0xDEADBEEF;
    *b = 0xCAFEBABE;
    if (*a == 0xDEADBEEF && *b == 0xCAFEBABE && a != b)
        klog("KMALLOC_OK\n");
    else
        klog("KMALLOC_FAIL\n");

    // Phase 3, Item 9 — cooperative multithreading.
    sched_init();
    task_create(thread_a);
    task_create(thread_b);
    while (a_runs < 5 || b_runs < 5) sched_yield();
    serial_write("\n");
    if (a_runs >= 5 && b_runs >= 5) klog("THREADS_OK\n");
    else                            klog("THREADS_FAIL\n");

    // Phase 3, Item 10 — preemptive scheduling. Spawn two workers that never
    // yield, then spin for ~30 ticks (300 ms). Both counters can only advance
    // if the timer is preempting them.
    task_create(worker_p);
    task_create(worker_q);
    uint32_t t0 = pit_get_ticks();
    while (pit_get_ticks() - t0 < 30) __asm__ volatile ("hlt");
    stop_workers = 1;
    for (int i = 0; i < 4; i++) sched_yield();   // let the workers exit
    if (work_p > 0 && work_q > 0) klog("SCHED_OK\n");
    else                          klog("SCHED_FAIL\n");

    // Phase 3, Item 11 — multiprocessing with isolated address spaces.
    task_t *pa = task_create(proc_a);
    pa->cr3 = process_create_space(MP_VADDR);
    task_t *pb = task_create(proc_b);
    pb->cr3 = process_create_space(MP_VADDR);
    while (!mp_a || !mp_b) sched_yield();
    if (mp_a && mp_b) klog("MP_OK\n");
    else              klog("MP_FAIL\n");

    if (magic != 0x2BADB002) {
        klog("BAD_MAGIC\n");
    }

#ifdef RUN_TESTS_ONLY
    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
#else
    shell_run();   // interactive REPL, never returns
    for (;;) { __asm__ volatile ("hlt"); }   // unreachable safety net
#endif
}
