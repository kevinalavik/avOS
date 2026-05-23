CodeSegment equ GdtCode - Gdt
DataSegment equ GdtData - Gdt
CodeSegment16 equ GdtCode16 - Gdt
DataSegment16 equ GdtData16 - Gdt

%macro SetupRealModeSegments 1
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, %1
%endmacro

%macro EnterProtectedMode 1
    lgdt [GdtDescriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CodeSegment:%1
%endmacro

%macro SetupProtectedModeSegments 1
    mov ax, DataSegment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, %1
%endmacro

%macro HaltCpu 0
%%loop:
    cli
    hlt
    jmp %%loop
%endmacro

%macro EmitFlatGdt 0
align 8
Gdt:
    dq 0                                      ; 0x00: null

GdtCode:                                      ; 0x08: 32-bit code
    dw 0xffff
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0

GdtData:                                      ; 0x10: 32-bit data
    dw 0xffff
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0

GdtCode16:                                    ; 0x18: 16-bit code
    dw 0xffff
    dw 0
    db 0
    db 10011010b
    db 00001111b
    db 0

GdtData16:                                    ; 0x20: 16-bit data
    dw 0xffff
    dw 0
    db 0
    db 10010010b
    db 00001111b
    db 0

GdtDescriptor:
    dw GdtDescriptor - Gdt - 1
    dd Gdt
%endmacro
