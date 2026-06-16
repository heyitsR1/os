#include "keyboard.h"
#include "isr.h"
#include "pic.h"
#include "../include/io.h"

#define KB_DATA   0x60

// Scancode set 1: unshifted make codes 0x01-0x39
static const char sc_lower[58] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.',
    '/', 0, '*', 0, ' '
};

// Shifted equivalents
static const char sc_upper[58] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>',
    '?', 0, '*', 0, ' '
};

#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_CAPS    0x3A
#define SC_BREAK   0x80   // bit set on key release

static int shift_held = 0;
static int caps_lock  = 0;

#define BUF_SIZE 64
static volatile char     buf[BUF_SIZE];
static volatile uint32_t head = 0;
static volatile uint32_t tail = 0;

static void kb_handler(registers_t *r) {
    (void)r;
    uint8_t sc = inb(KB_DATA);

    int released = sc & SC_BREAK;
    uint8_t key  = sc & ~SC_BREAK;

    // Track modifier keys
    if (key == SC_LSHIFT || key == SC_RSHIFT) {
        shift_held = !released;
        return;
    }
    if (key == SC_CAPS && !released) {
        caps_lock = !caps_lock;
        return;
    }

    if (released) return;   // ignore all other break codes

    if (key >= sizeof(sc_lower)) return;

    // Determine effective shift: XOR of shift key and caps-lock
    // (caps-lock only inverts letters; for other keys only shift_held matters)
    char lo = sc_lower[key];
    char hi = sc_upper[key];
    char c;
    if (lo >= 'a' && lo <= 'z') {
        c = (shift_held ^ caps_lock) ? hi : lo;
    } else {
        c = shift_held ? hi : lo;
    }

    if (c) {
        uint32_t next = (head + 1) % BUF_SIZE;
        if (next != tail) {
            buf[head] = c;
            head = next;
        }
    }
}

void keyboard_init(void) {
    irq_install_handler(1, kb_handler);
    pic_clear_mask(1);
}

char keyboard_getc(void) {
    if (tail == head) return 0;
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
