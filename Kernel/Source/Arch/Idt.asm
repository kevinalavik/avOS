bits 64
section .text

extern IdtHandle
global IdtRestoreFrame

IsrCommon:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr4
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rax, cr0
    push rax

    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    mov rdi, rsp
    call IdtHandle
    mov rsp, rax

IdtRestoreFrame:
    pop rcx
    mov es, cx
    pop rcx
    mov ds, cx

    pop rcx
    mov cr0, rcx
    pop rcx
    mov cr2, rcx
    pop rcx
    mov cr3, rcx
    pop rcx
    mov cr4, rcx

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16
    iretq

%assign i 0
%rep 256
IsrWrapper%+i:
    %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 29 || i == 30
        push i
    %else
        push 0
        push i
    %endif
    jmp IsrCommon
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits

section .data
global IsrWrapperTable
IsrWrapperTable:
%assign i 0
%rep 256
    dq IsrWrapper%+i
%assign i i+1
%endrep
