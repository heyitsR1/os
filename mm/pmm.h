#pragma once
#include "../include/types.h"

// Physical memory manager — bitmap allocator, 4 KiB pages.
void     pmm_init(uint32_t mb_info_addr);
uint32_t pmm_alloc_frame(void);   // returns physical address, 0 if OOM
void     pmm_free_frame(uint32_t addr);
uint32_t pmm_free_count(void);
