bits 32

global EnterLongMode

LongModeCodeSegment equ LongModeCode - LongModeGdt
LongModeDataSegment equ LongModeData - LongModeGdt

section .text

EnterLongMode:
    mov ebp, [esp + 4]
    mov esi, [esp + 8]
    mov ebx, [esp + 12]
    mov edi, [esp + 16]

    lgdt [LongModeGdtDescriptor]

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov cr3, ebp

    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp LongModeCodeSegment:LongModeEntry

bits 64

LongModeEntry:
    mov ax, LongModeDataSegment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x90000
    xor rbp, rbp

    mov eax, esi
    mov edx, ebx
    shl rdx, 32
    or rax, rdx
    mov edi, edi
    jmp rax

bits 32

section .rodata

align 8
LongModeGdt:
    dq 0

LongModeCode:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 10100000b
    db 0

LongModeData:
    dw 0
    dw 0
    db 0
    db 10010010b
    db 00000000b
    db 0

LongModeGdtDescriptor:
    dw LongModeGdtDescriptor - LongModeGdt - 1
    dd LongModeGdt

section .note.GNU-stack noalloc noexec nowrite progbits
