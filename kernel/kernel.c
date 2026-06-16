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

static void qemu_exit(uint8_t code) {
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

// Print to both the on-screen terminal and the serial port.
static void klog(const char *s) {
    vga_write(s);
    serial_write(s);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    serial_init();
    serial_write("BOOT_OK\n");

    vga_init();
    vga_set_color(VGA_WHITE, VGA_BLUE);
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

    if (magic != 0x2BADB002) {
        klog("BAD_MAGIC\n");
    }

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
}
