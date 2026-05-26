bits 64

global SchedulerEnterFrame
global SchedulerLaunch
extern IdtRestoreFrame
extern SchedulerBootstrap

section .text
SchedulerEnterFrame:
    mov rsp, rdi
    jmp IdtRestoreFrame

SchedulerLaunch:
    mov rsp, rsi
    xor rbp, rbp
    sti
    call SchedulerBootstrap
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
