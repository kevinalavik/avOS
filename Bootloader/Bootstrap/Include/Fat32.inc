SectorBuffer equ 0x0500

InitializeFat32:
    mov ax, [PartitionLba]
    mov bx, SectorBuffer
    mov cl, 1
    call ReadSectors

    mov ax, [SectorBuffer + 0x0e]
    add ax, [PartitionLba]
    mov [FatLba], ax

    mov al, [SectorBuffer + 0x0d]
    mov [SectorsPerCluster], al

    mov ax, [SectorBuffer + 0x24]
    xor cx, cx
    mov cl, [SectorBuffer + 0x10]
    mul cx
    add ax, [FatLba]
    mov [DataLba], ax

    mov ax, [SectorBuffer + 0x2c]
    mov [RootCluster], ax
    ret

; DS:SI = 11-byte FAT 8.3 name, AX = first cluster
Fat32FindFile:
    mov [FileName], si
    mov ax, [RootCluster]
    mov [DirectoryCluster], ax

.cluster:
    mov ax, [DirectoryCluster]
    cmp ax, 0xfff8
    jae .missing

    mov bx, SectorBuffer
    call Fat32LoadCluster

    xor cx, cx
    mov cl, [SectorsPerCluster]
    shl cx, 4
    mov di, SectorBuffer

.entry:
    mov al, [di]
    test al, al
    jz .missing
    cmp al, 0xe5
    je .next

    mov al, [di + 11]
    test al, 0x18
    jnz .next

    push di
    mov si, [FileName]
    push cx
    mov cx, 11
    repe cmpsb
    pop cx
    pop di
    je .found

.next:
    add di, 32
    loop .entry

    mov ax, [DirectoryCluster]
    call Fat32ReadNextCluster
    mov [DirectoryCluster], ax
    jmp .cluster

.missing:
    stc
    ret

.found:
    mov ax, [di + 26]
    clc
    ret

; AX = cluster, ES:BX = destination
Fat32LoadCluster:
    push bx
    sub ax, 2
    xor cx, cx
    mov cl, [SectorsPerCluster]
    mul cx
    add ax, [DataLba]
    pop bx

    mov cl, [SectorsPerCluster]
    call ReadSectors
    ret

; AX = cluster, AX = next cluster
Fat32ReadNextCluster:
    mov bx, ax
    shr ax, 7
    add ax, [FatLba]

    push bx
    mov bx, SectorBuffer
    mov cl, 1
    call ReadSectors
    pop bx

    and bx, 0x007f
    shl bx, 2
    mov ax, [SectorBuffer + bx]
    ret

FileName dw 0
FatLba dw 0
DataLba dw 0
RootCluster dw 0
DirectoryCluster dw 0
SectorsPerCluster db 0
