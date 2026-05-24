bits 16

global Stage2Entry
global GdtDescriptor
global BootMemoryMapTable
extern Stage2BootDrive
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
    mov [Stage2BootDrive], dl

    call SetTextMode80x25
    call EnableA20
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

section .data

align 4
BootMemoryMapTable:
    dd 0
BootMemoryMapEntries:
    times MemoryMapMaxEntries * MemoryMapEntrySize db 0

section .note.GNU-stack noalloc noexec nowrite progbits
