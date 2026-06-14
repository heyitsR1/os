#pragma once
#include "../include/types.h"

// Register snapshot pushed by the common stub. Field order MUST match the
// push sequence in isr_stubs.asm (see comments there).
typedef struct registers {
    uint32_t ds;                                     // pushed by stub
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha
    uint32_t int_no, err_code;                       // pushed by stub
    uint32_t eip, cs, eflags, useresp, ss;           // pushed by CPU
} registers_t;

typedef void (*isr_handler_t)(registers_t *);

void isr_install(void);                          // register exception gates 0-31
void irq_install(void);   // register IRQ gates 32-47 (call after pic_remap)
void irq_install_handler(int irq, isr_handler_t h);
void irq_uninstall_handler(int irq);

// Called from assembly stubs.
void isr_handler(registers_t *r);
void irq_handler(registers_t *r);
