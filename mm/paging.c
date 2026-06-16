#include "paging.h"

// Identity-map the first 16 MiB (4 page tables × 4 MiB each).
// All static so they exist before kmalloc is available.
#define MAP_TABLES 4

static uint32_t page_dir[1024]              __attribute__((aligned(4096)));
static uint32_t page_tables[MAP_TABLES][1024] __attribute__((aligned(4096)));

void paging_init(void) {
    // Clear the directory (all not-present).
    for (int i = 0; i < 1024; i++) page_dir[i] = 0;

    // Build identity-mapping page tables: virt == phys for 0..16 MiB.
    for (int t = 0; t < MAP_TABLES; t++) {
        for (int p = 0; p < 1024; p++) {
            uint32_t phys = (uint32_t)(t * 1024 + p) * PAGE_SIZE;
            page_tables[t][p] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
        page_dir[t] = (uint32_t)page_tables[t] | PAGE_PRESENT | PAGE_WRITE;
    }

    // Load CR3 and enable paging (PG bit in CR0).
    __asm__ volatile (
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or  $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        : : "r"(page_dir) : "eax"
    );

}

uint32_t paging_kernel_dir(void) {
    return (uint32_t)page_dir;
}
