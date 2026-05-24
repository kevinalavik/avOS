bits 16
org 0x7c00

Stage2LoadSegment equ 0x0000
Stage2LoadOffset equ 0x7e00

Entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    call InitializeDisk
    call InitializeFat32
    call LoadStage2

EnterStage2:
    mov dl, [BootDrive]
    jmp Stage2LoadSegment:Stage2LoadOffset

LoadStage2:
    mov si, Stage2Name
    call Fat32FindFile
    jc Hang

    mov [Stage2Cluster], ax
    mov word [Stage2WriteOffset], Stage2LoadOffset

.cluster:
    mov ax, [Stage2Cluster]
    cmp ax, 0xfff8
    jae .done

    mov bx, [Stage2WriteOffset]
    call Fat32LoadCluster

    xor ax, ax
    mov al, [SectorsPerCluster]
    mov cx, 9
    shl ax, cl
    add [Stage2WriteOffset], ax

    mov ax, [Stage2Cluster]
    call Fat32ReadNextCluster
    mov [Stage2Cluster], ax
    jmp .cluster

.done:
    ret

DiskError:
Hang:
    hlt
    jmp Hang

%include "Bootstrap/Include/Disk.inc"
%include "Bootstrap/Include/Fat32.inc"

Stage2Name db "STAGE2  BIN"
Stage2Cluster dw 0
Stage2WriteOffset dw 0

times 446 - ($ - $$) db 0
times 510 - ($ - $$) db 0
dw 0xaa55
