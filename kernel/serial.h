#pragma once
#include "../include/types.h"

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
void serial_write_uint(uint32_t n);   // decimal, no leading zeros
