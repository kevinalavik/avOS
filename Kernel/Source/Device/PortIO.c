#include <Device/PortIO.h>

uint8_t PortIORead8(uint16_t Port)
{
	uint8_t Value;

	__asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));

	return Value;
}

uint16_t PortIORead16(uint16_t Port)
{
	uint16_t Value;
	__asm__ volatile("inw %1, %0" : "=a"(Value) : "Nd"(Port));
	return Value;
}

uint32_t PortIORead32(uint16_t Port)
{
	uint32_t Value;
	__asm__ volatile("inl %1, %0" : "=a"(Value) : "Nd"(Port));
	return Value;
}

void PortIOWrite8(uint16_t Port, uint8_t Value)
{
	__asm__ volatile("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

void PortIOWrite16(uint16_t Port, uint16_t Value)
{
	__asm__ volatile("outw %0, %1" : : "a"(Value), "Nd"(Port));
}

void PortIOWrite32(uint16_t Port, uint32_t Value)
{
	__asm__ volatile("outl %0, %1" : : "a"(Value), "Nd"(Port));
}

void PortIOWait(void)
{
	/* 0x80 is historically unused; this gives a tiny I/O delay. */
	__asm__ volatile("outb %%al, $0x80" : : "a"(0));
}
