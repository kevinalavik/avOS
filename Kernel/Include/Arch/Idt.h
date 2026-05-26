#ifndef ARCH_IDT_H
#define ARCH_IDT_H

#include <stdint.h>

typedef struct IdtEntry {
	uint16_t BaseLow;
	uint16_t CodeSeg;
	uint8_t Ist;
	uint8_t Flags;
	uint16_t BaseMid;
	uint32_t BaseHigh;
	uint32_t Reserved;
} __attribute__((packed)) IdtEntry;

typedef struct Idtr {
	uint16_t Limit;
	uint64_t Base;
} __attribute__((packed)) Idtr;

typedef struct Frame {
	uint64_t es;
	uint64_t ds;

	uint64_t cr0;
	uint64_t cr2;
	uint64_t cr3;
	uint64_t cr4;

	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	uint64_t vector;
	uint64_t err;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
} __attribute__((packed)) Frame;

const char *ExceptionName(uint64_t Vector);
void IdtInit(void);

#endif
