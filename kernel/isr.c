#include "isr.h"
#include "idt.h"
#include "serial.h"
#include "vga.h"

// Declared in isr_stubs.asm.
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

static void (*const isr_stubs[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static const char *const exception_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound range", "Invalid opcode", "Device N/A",
    "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection", "Page fault", "Reserved",
    "x87 FPU error", "Alignment check", "Machine check", "SIMD FP",
    "Virtualization", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
};

static isr_handler_t irq_handlers[16] = { 0 };

static void klog(const char *s) { vga_write(s); serial_write(s); }

void isr_install(void) {
    for (int i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, (uint32_t)isr_stubs[i], 0x08, 0x8E);
}

void irq_install_handler(int irq, isr_handler_t h) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = h;
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = 0;
}

void isr_handler(registers_t *r) {
    if (r->int_no == 3) {            // breakpoint: used by the smoke test
        klog("ISR3_OK\n");
        return;                      // resume normally
    }
    klog("EXCEPTION: ");
    if (r->int_no < 32) klog(exception_names[r->int_no]);
    klog("\nHALT\n");
    for (;;) { __asm__ volatile ("cli; hlt"); }
}

// PIC end-of-interrupt is sent here; declared in pic.h (Task 5).
extern void pic_send_eoi(uint8_t irq);

void irq_handler(registers_t *r) {
    uint8_t irq = (uint8_t)(r->int_no - 32);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);
    pic_send_eoi(irq);
}

// TEMPORARY stub, replaced by pic.c in Task 5.
__attribute__((weak)) void pic_send_eoi(uint8_t irq) { (void)irq; }
