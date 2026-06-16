ENTRY(_start)

SECTIONS
{
    . = 1M;                    /* load the kernel at 1 MiB */

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)          /* header must be in the first 8 KiB */
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }

    _kernel_end = .;
}
