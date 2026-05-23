#include <Arch/Gdt.h>
#include <stddef.h>

enum {
	GdtAccessPresent = 0x80,
	GdtAccessRing0 = 0x00,
	GdtAccessRing3 = 0x60,
	GdtAccessCodeOrData = 0x10,
	GdtAccessExecutable = 0x08,
	GdtAccessReadWrite = 0x02,

	GdtFlagGranularity4K = 0x80,
	GdtFlagLongMode = 0x20,

	GdtKernelCodeSelector = 0x08,
	GdtKernelDataSelector = 0x10,
	GdtTssSelector = 0x28,
};

#define GdtEntry(Base, Limit, Access, Flags)                          \
	{                                                                 \
		(uint16_t)((Limit) & 0xFFFFu), (uint16_t)((Base) & 0xFFFFu),  \
			(uint8_t)(((Base) >> 16) & 0xFFu), (uint8_t)(Access),     \
			(uint8_t)((((Limit) >> 16) & 0x0Fu) | ((Flags) & 0xF0u)), \
			(uint8_t)(((Base) >> 24) & 0xFFu)                         \
	}

typedef struct Tss {
	uint32_t Reserved0;
	uint64_t Rsp[3];
	uint64_t Reserved1;
	uint64_t Ist[7];
	uint64_t Reserved2;
	uint16_t Reserved3;
	uint16_t IoMapBase;
} __attribute__((packed)) Tss;

static Gdt KernelGdt __attribute__((aligned(8))) = {
	.Entries = {
		GdtEntry(0, 0, 0, 0),
		GdtEntry(0, 0xFFFFu,
				 GdtAccessPresent | GdtAccessRing0 | GdtAccessCodeOrData |
					 GdtAccessExecutable | GdtAccessReadWrite,
				 GdtFlagGranularity4K | GdtFlagLongMode),
		GdtEntry(0, 0xFFFFu,
				 GdtAccessPresent | GdtAccessRing0 | GdtAccessCodeOrData |
					 GdtAccessReadWrite,
				 GdtFlagGranularity4K),
		GdtEntry(0, 0xFFFFu,
				 GdtAccessPresent | GdtAccessRing3 | GdtAccessCodeOrData |
					 GdtAccessExecutable | GdtAccessReadWrite,
				 GdtFlagGranularity4K | GdtFlagLongMode),
		GdtEntry(0, 0xFFFFu,
				 GdtAccessPresent | GdtAccessRing3 | GdtAccessCodeOrData |
					 GdtAccessReadWrite,
				 GdtFlagGranularity4K),
		GdtEntry(0, 0, 0, 0),
		GdtEntry(0, 0, 0, 0),
	},
};

static Gdtr KernelGdtr = {
	.Limit = sizeof(KernelGdt.Entries) - 1u,
	.Base = (uint64_t)&KernelGdt.Entries,
};

static Tss KernelTss;

static void ClearMemory(void *Buffer, size_t Size)
{
	uint8_t *Bytes = Buffer;

	for (size_t Index = 0; Index < Size; ++Index) {
		Bytes[Index] = 0;
	}
}

static void GdtSetTssDescriptor(Tss *State)
{
	uint64_t Base = (uint64_t)State;
	uint64_t Limit = sizeof(*State) - 1u;
	uint64_t Low = 0;
	uint64_t *RawEntries = (uint64_t *)KernelGdt.Entries;

	Low |= Limit & 0xFFFFu;
	Low |= (Base & 0xFFFFu) << 16;
	Low |= ((Base >> 16) & 0xFFu) << 32;
	Low |= 0x89ull << 40;
	Low |= ((Limit >> 16) & 0x0Fu) << 48;
	Low |= ((Base >> 24) & 0xFFu) << 56;

	RawEntries[5] = Low;
	RawEntries[6] = Base >> 32;
}

void GdtInit(void)
{
	__asm__ volatile(
		"lgdt %[Gdtr]\n"
		"pushq %[CodeSelector]\n"
		"leaq 1f(%%rip), %%rax\n"
		"pushq %%rax\n"
		"lretq\n"
		"1:\n"
		"movw %[DataSelector], %%ax\n"
		"movw %%ax, %%ds\n"
		"movw %%ax, %%es\n"
		"movw %%ax, %%ss\n"
		"movw %%ax, %%fs\n"
		"movw %%ax, %%gs\n"
		:
		: [Gdtr] "m"(KernelGdtr), [CodeSelector] "i"(GdtKernelCodeSelector),
		  [DataSelector] "i"(GdtKernelDataSelector)
		: "rax", "memory");
}

void GdtTssInit(uint64_t Rsp0)
{
	ClearMemory(&KernelTss, sizeof(KernelTss));
	KernelTss.Rsp[0] = Rsp0;
	KernelTss.IoMapBase = sizeof(KernelTss);
	GdtSetTssDescriptor(&KernelTss);

	__asm__ volatile("ltr %%ax" ::"a"((uint16_t)GdtTssSelector) : "memory");
}

void GdtSetKernelStack(uint64_t Rsp0)
{
	KernelTss.Rsp[0] = Rsp0;
}
