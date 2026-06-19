; Hand-written context switch for cooperative/preemptive kernel threads.
;
; The System V cdecl ABI says EBP, EBX, ESI, EDI are callee-saved. By pushing
; them here and saving ESP, a task's entire CPU context is captured on its own
; stack. Restoring is the mirror image, and the final `ret` jumps to whatever
; EIP the new task had saved.

section .text
global context_switch
global thread_trampoline

; void context_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);
;   [esp+4]  = old_esp   (where to store the outgoing task's ESP)
;   [esp+8]  = new_esp   (the incoming task's saved ESP)
;   [esp+12] = new_cr3   (the incoming task's page directory; 0 = leave CR3)
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    ; After 4 pushes the args have shifted up by 16 bytes.
    mov eax, [esp + 20]    ; old_esp pointer
    mov [eax], esp         ; *old_esp = current ESP (context now fully saved)
    mov edx, [esp + 24]    ; incoming ESP
    mov ecx, [esp + 28]    ; incoming CR3
    mov esp, edx           ; switch onto the new stack
    ; Switch address spaces. Writing CR3 flushes the TLB, so skip it when the
    ; task carries no directory of its own (cr3 == 0).
    test ecx, ecx
    jz .skip_cr3
    mov cr3, ecx
.skip_cr3:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                    ; jump to the incoming task's saved EIP

; A freshly created thread's first `ret` from context_switch lands here. We
; enable interrupts (a brand-new thread has none of an iret frame to restore
; IF for it) and then `ret` again into the thread's entry function, which sits
; just above this address on the prepared stack.
thread_trampoline:
    sti
    ret
