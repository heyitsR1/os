#pragma once
#include "../include/types.h"

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE   (1 << 1)
#define PAGE_SIZE    4096

void paging_init(void);
