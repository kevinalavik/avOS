.section .text
.global _start
.type _start, @function

_start:
	xor %rbp, %rbp
	andq $-16, %rsp
	call main
	mov %eax, %edi
	call Exit

.Lhang:
	hlt
	jmp .Lhang

.section .note.GNU-stack,"",@progbits
