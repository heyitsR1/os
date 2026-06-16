#include "kmalloc.h"

extern uint32_t _kernel_end;   // from linker.ld

static uint32_t heap_ptr  = 0;
static uint32_t heap_base = 0;

void kmalloc_init(void) {
    // Start the heap at the first page-aligned address after the kernel image.
    heap_base = ((uint32_t)&_kernel_end + 0xFFF) & ~0xFFFu;
    heap_ptr  = heap_base;
}

// Align heap_ptr up to `align` (must be a power of two), then bump.
void *kmalloc_aligned(uint32_t size, uint32_t align) {
    heap_ptr = (heap_ptr + align - 1) & ~(align - 1);
    void *ptr = (void *)heap_ptr;
    heap_ptr += size;
    return ptr;
}

void *kmalloc(uint32_t size) {
    return kmalloc_aligned(size, 8);   // 8-byte default alignment
}

void kfree(void *ptr) { (void)ptr; }  // bump allocator; freed in Phase 3
