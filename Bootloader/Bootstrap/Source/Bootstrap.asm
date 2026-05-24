bits 16
org 0x7c00

Stage2LoadSegment equ 0x0000
Stage2LoadOffset equ 0x7e00

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 64
%endif

Entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [BootDrive], dl
    call LoadStage2Fixed

EnterStage2:
    mov dl, [BootDrive]
    jmp Stage2LoadSegment:Stage2LoadOffset


; Load stage2 from fixed LBAs starting at LBA 1.
LoadStage2Fixed:
    push ax
    push bx
    push cx
    push dx
    push si

    mov word [DiskPacket.count], STAGE2_SECTORS
    mov word [DiskPacket.offset], Stage2LoadOffset
    mov word [DiskPacket.segment], Stage2LoadSegment
    mov dword [DiskPacket.lba], 1
    mov dword [DiskPacket.lba + 4], 0

    mov ah, 0x42
    mov dl, [BootDrive]
    mov si, DiskPacket
    int 0x13
    jc Hang

    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

Hang:
    hlt
    jmp Hang

BootDrive db 0

DiskPacket:
    db 0x10
    db 0
.count:
    dw 0
.offset:
    dw 0
.segment:
    dw 0
.lba:
    dq 0

times 446 - ($ - $$) db 0
times 510 - ($ - $$) db 0
dw 0xaa55
