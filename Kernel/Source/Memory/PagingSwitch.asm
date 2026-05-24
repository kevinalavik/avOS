bits 64

global PagingInit

extern PagingBuild

section .text

BootStackTop equ 0x90000

; bool PagingInit(const BootInfo *info, uint64_t new_stack_top)
PagingInit:
    push rbx
    mov rbx, rsi

    call PagingBuild
    test rax, rax
    jz .fail

    mov r8, rax
    mov r9, rbx
    pop rbx

    mov r10, rsp
    mov r11, BootStackTop
    cmp r10, r11
    jae .fail_after_pop

    mov rcx, r11
    sub rcx, r10
    mov rdx, r9
    sub rdx, rcx

    mov rsi, r10
    mov rdi, rdx
    cld
    rep movsb

    cmp rbp, r10
    jb .skip_rbp
    cmp rbp, r11
    jae .skip_rbp
    sub r9, rcx
    sub r9, r10
    add rbp, r9

.skip_rbp:
    mov cr3, r8
    mov rsp, rdx
    mov eax, 1
    ret

.fail:
    pop rbx
.fail_after_pop:
    xor eax, eax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
