#include "serial.h"
#include "../include/io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); // disable interrupts
    outb(COM1 + 3, 0x80); // enable DLAB (baud divisor)
    outb(COM1 + 0, 0x03); // divisor low  = 3 (38400 baud)
    outb(COM1 + 1, 0x00); // divisor high = 0
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static int serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    if (c == '\n') {            // make raw newlines render on terminals
        while (!serial_tx_empty()) {}
        outb(COM1, '\r');
    }
    while (!serial_tx_empty()) {}
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    for (; *s; s++) serial_putc(*s);
}

void serial_write_uint(uint32_t n) {
    if (n == 0) { serial_putc('0'); return; }
    char tmp[11];
    int  i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    while (i--) serial_putc(tmp[i]);
}
