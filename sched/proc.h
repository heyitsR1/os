#pragma once
#include "../include/types.h"

// Build a private address space for a "process". The new page directory
// shares the kernel's identity-mapped low memory (so kernel code and the
// task's stack stay reachable) but maps `vaddr` to a freshly allocated
// physical frame that no other process can see. Returns the directory's
// physical address, ready to load into a task's cr3.
uint32_t process_create_space(uint32_t vaddr);
