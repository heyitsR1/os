#pragma once
#include "../include/types.h"

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE   (1 << 1)
#define PAGE_SIZE    4096

void paging_init(void);

// Physical address of the kernel's page directory. Identity-mapped low
// memory means this value is both the virtual and physical address, so it
// can be loaded straight into CR3.
uint32_t paging_kernel_dir(void);
