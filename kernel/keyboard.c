#include "keyboard.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"

#define KB_DATA   0x60

// US QWERTY: scancode set 1 make codes 0x01-0x39
static const char sc_ascii[58] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00-0x09 */
    '9', '0', '-', '=', '\b', '\t',                     /* 0x0A-0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', /* 0x10-0x19 */
    '[', ']', '\n', 0,                                   /* 0x1A-0x1D (0=Ctrl) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 0x1E-0x27 */
    '\'', '`', 0,                                        /* 0x28-0x2A (0=LShift) */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', /* 0x2B-0x34 */
    '/', 0,                                              /* 0x35-0x36 (0=RShift) */
    '*', 0, ' '                                          /* 0x37-0x39 */
};

#define BUF_SIZE 64
static volatile char     buf[BUF_SIZE];
static volatile uint32_t head = 0;  // write index
static volatile uint32_t tail = 0;  // read index

static void kb_handler(registers_t *r) {
    (void)r;
    uint8_t sc = inb(KB_DATA);

    if (sc & 0x80) return;  // break code (key release) — ignore

    if (sc < sizeof(sc_ascii)) {
        char c = sc_ascii[sc];
        if (c) {
            uint32_t next = (head + 1) % BUF_SIZE;
            if (next != tail) {   // drop if full
                buf[head] = c;
                head = next;
            }
        }
    }
}

void keyboard_init(void) {
    irq_install_handler(1, kb_handler);
    pic_clear_mask(1);   // unmask IRQ1
}

char keyboard_getc(void) {
    if (tail == head) return 0;
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
