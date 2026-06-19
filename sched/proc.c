#include "proc.h"
#include "../mm/paging.h"
#include "../mm/pmm.h"
#include "../mm/kmalloc.h"

// 1024 entries each in a page directory and a page table.
#define ENTRIES        1024
// Number of low page-directory entries that cover the kernel's identity map
// (4 tables x 4 MiB = first 16 MiB). Must match mm/paging.c.
#define KERNEL_PDES    4

uint32_t process_create_space(uint32_t vaddr) {
    // kmalloc lives in identity-mapped low memory, so a directory/table's
    // virtual address is also its physical address — exactly what CR3 wants.
    uint32_t *dir = (uint32_t *)kmalloc_aligned(ENTRIES * 4, PAGE_SIZE);
    uint32_t *kdir = (uint32_t *)paging_kernel_dir();

    for (int i = 0; i < ENTRIES; i++) dir[i] = 0;
    // Share the kernel's low identity map so code and stacks stay mapped.
    for (int i = 0; i < KERNEL_PDES; i++) dir[i] = kdir[i];

    // Map vaddr -> a brand-new physical frame, private to this directory.
    uint32_t *pt = (uint32_t *)kmalloc_aligned(ENTRIES * 4, PAGE_SIZE);
    for (int i = 0; i < ENTRIES; i++) pt[i] = 0;

    uint32_t frame = pmm_alloc_frame();
    uint32_t pde   = vaddr >> 22;            // top 10 bits -> directory index
    uint32_t pte   = (vaddr >> 12) & 0x3FF;  // next 10 bits -> table index

    pt[pte]  = frame      | PAGE_PRESENT | PAGE_WRITE;
    dir[pde] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITE;

    return (uint32_t)dir;
}
