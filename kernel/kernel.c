#include "../include/types.h"
#include "../include/io.h"
#include "serial.h"
#include "vga.h"
#include "gdt.h"

static void qemu_exit(uint8_t code) {
    outb(0xF4, code);
}

// Print to both the on-screen terminal and the serial port.
static void klog(const char *s) {
    vga_write(s);
    serial_write(s);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    serial_write("BOOT_OK\n");

    vga_init();
    vga_set_color(VGA_WHITE, VGA_BLUE);
    klog("omen OS - phase 1\n");
    klog("VGA_OK\n");

    gdt_init();
    klog("GDT_OK\n");

    if (magic != 0x2BADB002) {
        klog("BAD_MAGIC\n");
    }

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
}
