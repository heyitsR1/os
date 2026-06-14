; Multiboot1 header + kernel entry point.
bits 32

MBALIGN  equ 1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1            ; provide a memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002        ; Multiboot1 magic
CHECKSUM equ -(MAGIC + MBFLAGS)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top         ; set up the stack (grows downward)
    push ebx                   ; arg2: pointer to multiboot info struct
    push eax                   ; arg1: multiboot magic (0x2BADB002)
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang
