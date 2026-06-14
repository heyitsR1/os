bits 32
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]   ; pointer to gdt_ptr
    lgdt [eax]

    mov ax, 0x10         ; data selector (entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.reload_cs  ; code selector (entry 1) - far jump reloads CS
.reload_cs:
    ret
