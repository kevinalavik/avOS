#ifndef ARCH_GDT_H
#define ARCH_GDT_H

#include <stdint.h>

typedef struct GdtDescriptor {
	uint16_t LimitLow;
	uint16_t BaseLow;
	uint8_t BaseMiddle;
	uint8_t Access;
	uint8_t LimitFlags;
	uint8_t BaseHigh;
} __attribute__((packed)) GdtDescriptor;

typedef struct Gdtr {
	uint16_t Limit;
	uint64_t Base;
} __attribute__((packed)) Gdtr;

typedef struct Gdt {
	GdtDescriptor Entries[7];
} __attribute__((packed)) Gdt;

void GdtInit(void);
void GdtTssInit(uint64_t Rsp0);
void GdtSetKernelStack(uint64_t Rsp0);

#endif
