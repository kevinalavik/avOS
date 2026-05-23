bits 16

global Stage2Entry
global BootMemoryMapTable
global VesaModeNumber
global VesaFramebufferAddress
global VesaWidth
global VesaHeight
global VesaPitch
global VesaBpp
extern S2Entry
extern __bss_start
extern __bss_end

%include "Bootstrap/Include/ProtectedMode.inc"

MemoryMapMaxEntries equ 32
MemoryMapEntrySize  equ 24

section .text.entry

Stage2Entry:
    cli
    SetupRealModeSegments 0x7c00

    call SetTextMode80x25
    call EnableA20
    call SetVesaModeBochs
    call CollectMemoryMap

    EnterProtectedMode ProtectedModeEntry

bits 32

ProtectedModeEntry:
    SetupProtectedModeSegments 0x90000
    call ClearBss
    call S2Entry
    HaltCpu

bits 16

SetTextMode80x25:
    mov ax, 0x0003
    int 0x10
    ret

SetVesaModeBochs:
    push ax
    push bx
    push dx

    ; Zero out all VESA output variables (dd = 32 bits, written as two words)
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

    ; --- Detect Bochs VBE ---
    ; Read VBE_DISPI_INDEX_ID (index 0); value must be >= 0xB0C0.
    mov dx, 0x01CE
    xor ax, ax             ; index 0
    out dx, ax
    mov dx, 0x01CF
    in  ax, dx
    cmp ax, 0xB0C0
    jb  .done              ; not Bochs VBE — leave variables at zero

    ; --- Disable VBE before changing resolution / BPP ---
    mov dx, 0x01CE
    mov ax, 0x0004         ; VBE_DISPI_INDEX_ENABLE
    out dx, ax
    mov dx, 0x01CF
    xor ax, ax             ; VBE_DISPI_DISABLED
    out dx, ax

    ; --- XRES = 1024 ---
    mov dx, 0x01CE
    mov ax, 0x0001         ; VBE_DISPI_INDEX_XRES
    out dx, ax
    mov dx, 0x01CF
    mov ax, 1024
    out dx, ax

    ; --- YRES = 768 ---
    mov dx, 0x01CE
    mov ax, 0x0002         ; VBE_DISPI_INDEX_YRES
    out dx, ax
    mov dx, 0x01CF
    mov ax, 768
    out dx, ax

    ; --- BPP = 32 ---
    mov dx, 0x01CE
    mov ax, 0x0003         ; VBE_DISPI_INDEX_BPP
    out dx, ax
    mov dx, 0x01CF
    mov ax, 32
    out dx, ax

    ; --- Enable with linear framebuffer ---
    mov dx, 0x01CE
    mov ax, 0x0004         ; VBE_DISPI_INDEX_ENABLE
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0x0041         ; VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLE
    out dx, ax

    ; --- Store results ---
    mov word [VesaModeNumber], 1
    mov word [VesaModeNumber + 2], 0

    mov word [VesaWidth], 1024
    mov word [VesaWidth + 2], 0

    mov word [VesaHeight], 768
    mov word [VesaHeight + 2], 0

    mov word [VesaBpp], 32
    mov word [VesaBpp + 2], 0

    ; Pitch = width * (bpp / 8) = 1024 * 4 = 4096 = 0x1000
    mov word [VesaPitch], 0x1000
    mov word [VesaPitch + 2], 0

    ; Bootstrap fallback. Stage2 later queries the Bochs/QEMU PCI BAR
    ; and replaces this with the real framebuffer aperture when available.
    mov word [VesaFramebufferAddress],     0x0000
    mov word [VesaFramebufferAddress + 2], 0xE000

.done:
    pop dx
    pop bx
    pop ax
    ret

EnableA20:
    in  al, 0x92
    or  al, 0x02
    and al, 0xfe
    out 0x92, al
    ret

CollectMemoryMap:
    push word 0
    pop  ds
    push word 0
    pop  es

    xor  ebx, ebx
    xor  bp,  bp
    mov  dword [BootMemoryMapTable], 0
    mov  di, BootMemoryMapEntries

.next:
    cmp  bp, MemoryMapMaxEntries
    jae  .done

    mov  dword [di + 20], 1
    mov  eax, 0xe820
    mov  edx, 0x534d4150
    mov  ecx, MemoryMapEntrySize
    int  0x15
    push word 0
    pop  ds
    push word 0
    pop  es
    jc   .done

    cmp  eax, 0x534d4150
    jne  .done
    cmp  ecx, 20
    jb   .done

    mov  eax, [di + 8]
    or   eax, [di + 12]
    jz   .skip

    add  di, MemoryMapEntrySize
    inc  bp

.skip:
    test ebx, ebx
    jne  .next

.done:
    xor  eax, eax
    mov  ax, bp
    mov  [BootMemoryMapTable], eax
    ret

bits 32

ClearBss:
    mov  edi, __bss_start
    mov  ecx, __bss_end
    sub  ecx, edi
    xor  eax, eax
    cld
    rep  stosb
    ret

EmitFlatGdt

align 4
VesaModeNumber:         dd 0
VesaFramebufferAddress: dd 0
VesaWidth:              dd 0
VesaHeight:             dd 0
VesaPitch:              dd 0
VesaBpp:                dd 0

section .data

align 4
BootMemoryMapTable:
    dd 0
BootMemoryMapEntries:
    times MemoryMapMaxEntries * MemoryMapEntrySize db 0

section .note.GNU-stack noalloc noexec nowrite progbits