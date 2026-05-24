bits 32

global BiosDiskReadSectors
global Stage2BootDrive
extern GdtDescriptor

CodeSegment equ 0x08
DataSegment equ 0x10
CodeSegment16 equ 0x18
DataSegment16 equ 0x20

section .text

BiosDiskReadSectors:
    mov eax, [esp + 4]
    mov [BiosDiskPacketLba], eax
    mov dword [BiosDiskPacketLba + 4], 0

    mov ax, [esp + 8]
    mov [BiosDiskPacketCount], ax

    mov ebx, [esp + 12]
    mov ax, bx
    and ax, 0x000f
    mov [BiosDiskPacketOffset], ax
    shr ebx, 4
    mov [BiosDiskPacketSegment], bx

    pushfd
    pushad
    mov [BiosDiskSavedEsp], esp

    cli
    jmp CodeSegment16:BiosDiskProtected16

bits 16

BiosDiskProtected16:
    mov ax, DataSegment16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x7000
    mov ss, ax

    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp 0x0000:BiosDiskRealMode

BiosDiskRealMode:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7000

    mov byte [BiosDiskStatus], 0
    mov ah, 0x42
    mov dl, [Stage2BootDrive]
    mov si, BiosDiskPacket
    sti
    int 0x13
    cli
    jc .done

    mov byte [BiosDiskStatus], 1

.done:
    lgdt [GdtDescriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CodeSegment:BiosDiskProtected32

bits 32

BiosDiskProtected32:
    mov ax, DataSegment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [BiosDiskSavedEsp]

    popad
    popfd
    movzx eax, byte [BiosDiskStatus]
    ret

section .data

Stage2BootDrive:        db 0
BiosDiskStatus:         db 0

align 4
BiosDiskSavedEsp:       dd 0
BiosDiskPacket:
    db 0x10
    db 0
BiosDiskPacketCount:   dw 0
BiosDiskPacketOffset:  dw 0
BiosDiskPacketSegment: dw 0
BiosDiskPacketLba:     dq 0

section .note.GNU-stack noalloc noexec nowrite progbits
