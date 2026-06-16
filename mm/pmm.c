#include "pmm.h"

#define PAGE_SIZE   4096
#define MAX_FRAMES  (256 * 1024)          // covers 1 GiB of physical RAM
#define BITMAP_SIZE (MAX_FRAMES / 32)     // 32 frames per uint32

// Each bit: 0 = free, 1 = used.
static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_frames = 0;
static uint32_t free_frames  = 0;

// Multiboot1 info — only the fields we need.
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) mb_info_t;

typedef struct {
    uint32_t size;
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t len_lo;
    uint32_t len_hi;
    uint32_t type;       // 1 = available
} __attribute__((packed)) mb_mmap_t;

static void frame_set(uint32_t frame) {
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static void frame_clear(uint32_t frame) {
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static int frame_test(uint32_t frame) {
    return (bitmap[frame / 32] >> (frame % 32)) & 1;
}

// Mark every page in [base, base+len) as free.
static void pmm_mark_free(uint32_t base, uint32_t len) {
    uint32_t frame = base / PAGE_SIZE;
    uint32_t end   = (base + len) / PAGE_SIZE;
    if (end > MAX_FRAMES) end = MAX_FRAMES;
    for (; frame < end; frame++) {
        if (frame_test(frame)) {
            frame_clear(frame);
            free_frames++;
        }
    }
    if (end > total_frames) total_frames = end;
}

// Mark every page in [base, base+len) as used.
static void pmm_mark_used(uint32_t base, uint32_t len) {
    uint32_t frame = base / PAGE_SIZE;
    uint32_t end   = ((base + len) + PAGE_SIZE - 1) / PAGE_SIZE;
    if (end > MAX_FRAMES) end = MAX_FRAMES;
    for (; frame < end; frame++) {
        if (!frame_test(frame)) {
            frame_set(frame);
            if (free_frames) free_frames--;
        }
    }
}

// Symbol provided by linker.ld
extern uint32_t _kernel_end;

void pmm_init(uint32_t mb_info_addr) {
    // Start fully used; selectively free what GRUB says is available.
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) bitmap[i] = 0xFFFFFFFF;

    mb_info_t *mb = (mb_info_t *)mb_info_addr;

    if (mb->flags & (1 << 6)) {   // mmap present
        uint32_t offset = 0;
        while (offset < mb->mmap_length) {
            mb_mmap_t *entry = (mb_mmap_t *)(mb->mmap_addr + offset);
            if (entry->addr_hi == 0 && entry->type == 1) {
                pmm_mark_free(entry->addr_lo, entry->len_lo);
            }
            offset += entry->size + sizeof(entry->size);
        }
    } else {
        // Fallback: trust mem_upper (KiB above 1 MiB)
        pmm_mark_free(0x100000, (uint32_t)mb->mem_upper * 1024);
    }

    // Re-reserve: frame 0 (null page) and everything the kernel occupies.
    pmm_mark_used(0, PAGE_SIZE);
    pmm_mark_used(0x100000, (uint32_t)&_kernel_end - 0x100000 + PAGE_SIZE);

}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] == 0xFFFFFFFF) continue;
        for (uint32_t bit = 0; bit < 32; bit++) {
            if (!((bitmap[i] >> bit) & 1)) {
                uint32_t frame = i * 32 + bit;
                if (frame >= total_frames) return 0;
                frame_set(frame);
                free_frames--;
                return frame * PAGE_SIZE;
            }
        }
    }
    return 0;   // OOM
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame < MAX_FRAMES && frame_test(frame)) {
        frame_clear(frame);
        free_frames++;
    }
}

uint32_t pmm_free_count(void) { return free_frames; }
