PartitionEntry equ 0x7dbe

InitializeDisk:
    mov [BootDrive], dl
    mov ax, [PartitionEntry + 8]
    mov [PartitionLba], ax
    ret

; AX = absolute LBA, CL = sector count, ES:BX = destination
ReadSectors:
    push ax
    push bx
    push cx
    push dx
    push si

    xor ch, ch
    mov [DiskPacket.count], cx
    mov [DiskPacket.offset], bx
    mov [DiskPacket.segment], es
    mov [DiskPacket.lba], ax
    mov word [DiskPacket.lba + 2], 0
    mov dword [DiskPacket.lba + 4], 0

    mov ah, 0x42
    mov dl, [BootDrive]
    mov si, DiskPacket
    int 0x13
    jc DiskError

    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

BootDrive db 0
PartitionLba dw 0

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
