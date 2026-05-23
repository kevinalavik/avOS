#include <Arch/Idt.h>
#include <Core/Log.h>
#include <Core/Panic.h>
#include <Library/Printf.h>
#include <stddef.h>

#define IdtEntryCount 256

extern void *IsrWrapperTable[IdtEntryCount];

static IdtEntry Idt[IdtEntryCount];
static Idtr KernelIdtr = {
	.Limit = sizeof(Idt) - 1u,
	.Base = (uint64_t)&Idt,
};

const char *ExceptionName(uint64_t Vector)
{
	switch (Vector) {
	case 0:
		return "Divide-by-zero";
	case 1:
		return "Debug";
	case 2:
		return "Non-maskable interrupt";
	case 3:
		return "Breakpoint";
	case 4:
		return "Overflow";
	case 5:
		return "Bound range exceeded";
	case 6:
		return "Invalid opcode";
	case 7:
		return "Device not available";
	case 8:
		return "Double fault";
	case 10:
		return "Invalid TSS";
	case 11:
		return "Segment not present";
	case 12:
		return "Stack-segment fault";
	case 13:
		return "General protection fault";
	case 14:
		return "Page fault";
	case 16:
		return "x87 floating-point exception";
	case 17:
		return "Alignment check";
	case 18:
		return "Machine check";
	case 19:
		return "SIMD floating-point exception";
	case 20:
		return "Virtualization exception";
	case 21:
		return "Control protection exception";
	default:
		return "Interrupt / IRQ";
	}
}

void IdtHandle(Frame *Frame)
{
	if (Frame->vector < 32) {
		Panic(Frame, ExceptionName(Frame->vector));
	}

	LogWarn("unhandled IRQ vector=%llu", (unsigned long long)Frame->vector);
}

static void IdtSetEntry(size_t Vector, void *Handler, uint8_t Ist,
						uint8_t Flags)
{
	uint64_t Base = (uint64_t)Handler;

	Idt[Vector].BaseLow = (uint16_t)(Base & 0xFFFFu);
	Idt[Vector].CodeSeg = 0x08;
	Idt[Vector].Ist = Ist;
	Idt[Vector].Flags = Flags;
	Idt[Vector].BaseMid = (uint16_t)((Base >> 16) & 0xFFFFu);
	Idt[Vector].BaseHigh = (uint32_t)((Base >> 32) & 0xFFFFFFFFu);
	Idt[Vector].Reserved = 0;
}

void IdtInit(void)
{
	for (size_t i = 0; i < IdtEntryCount; ++i) {
		IdtSetEntry(i, IsrWrapperTable[i], 0, 0x8E);
	}

	__asm__ volatile("lidt %[Idtr]" ::[Idtr] "m"(KernelIdtr) : "memory");
}
