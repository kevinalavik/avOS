bits 16

global SetVesaModeBochs
global VesaModeNumber
global VesaFramebufferAddress
global VesaWidth
global VesaHeight
global VesaPitch
global VesaBpp

section .text

SetVesaModeBochs:
    push ax
    push bx
    push dx

    xor ax, ax
    mov word [VesaModeNumber],         ax
    mov word [VesaModeNumber + 2],     ax
    mov word [VesaFramebufferAddress], ax
    mov word [VesaFramebufferAddress + 2], ax
    mov word [VesaWidth],              ax
    mov word [VesaWidth + 2],          ax
    mov word [VesaHeight],             ax
    mov word [VesaHeight + 2],         ax
    mov word [VesaPitch],              ax
    mov word [VesaPitch + 2],          ax
    mov word [VesaBpp],                ax
    mov word [VesaBpp + 2],            ax

    mov dx, 0x01CE
    xor ax, ax
    out dx, ax
    mov dx, 0x01CF
    in  ax, dx
    cmp ax, 0xB0C0
    jb  .done

    mov dx, 0x01CE
    mov ax, 0x0004
    out dx, ax
    mov dx, 0x01CF
    xor ax, ax
    out dx, ax

    mov dx, 0x01CE
    mov ax, 0x0001
    out dx, ax
    mov dx, 0x01CF
    mov ax, 1024
    out dx, ax

    mov dx, 0x01CE
    mov ax, 0x0002
    out dx, ax
    mov dx, 0x01CF
    mov ax, 768
    out dx, ax

    mov dx, 0x01CE
    mov ax, 0x0003
    out dx, ax
    mov dx, 0x01CF
    mov ax, 32
    out dx, ax

    mov dx, 0x01CE
    mov ax, 0x0004
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0x0041
    out dx, ax

    mov word [VesaModeNumber], 1
    mov word [VesaModeNumber + 2], 0

    mov word [VesaWidth], 1024
    mov word [VesaWidth + 2], 0

    mov word [VesaHeight], 768
    mov word [VesaHeight + 2], 0

    mov word [VesaBpp], 32
    mov word [VesaBpp + 2], 0

    mov word [VesaPitch], 0x1000
    mov word [VesaPitch + 2], 0

    mov word [VesaFramebufferAddress],     0x0000
    mov word [VesaFramebufferAddress + 2], 0xE000

.done:
    pop dx
    pop bx
    pop ax
    ret

section .data

align 4
VesaModeNumber:         dd 0
VesaFramebufferAddress: dd 0
VesaWidth:              dd 0
VesaHeight:             dd 0
VesaPitch:              dd 0
VesaBpp:                dd 0

section .note.GNU-stack noalloc noexec nowrite progbits
