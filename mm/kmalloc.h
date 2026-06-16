#pragma once
#include "../include/types.h"

void  kmalloc_init(void);
void *kmalloc(uint32_t size);
void *kmalloc_aligned(uint32_t size, uint32_t align);
void  kfree(void *ptr);    // no-op for now; reserved for Phase 3 free-list
