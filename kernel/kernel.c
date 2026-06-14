#include "../include/types.h"
#include "../include/io.h"
#include "serial.h"

// Cleanly terminate QEMU when the isa-debug-exit device is present
// (used by the smoke test). On real hardware / plain GRUB boot this
// writes to an unused port and is harmless; execution falls through
// to the halt loop below.
static void qemu_exit(uint8_t code) {
    outb(0xF4, code);
}

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    serial_write("BOOT_OK\n");

    if (magic != 0x2BADB002) {
        serial_write("BAD_MAGIC\n");
    }

    qemu_exit(0);
    for (;;) { __asm__ volatile ("hlt"); }
}
