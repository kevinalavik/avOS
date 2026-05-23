#include <Device/PortIO.h>

uint8_t PortIORead8(uint16_t Port)
{
	uint8_t Value;

	__asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));

	return Value;
}

void PortIOWrite8(uint16_t Port, uint8_t Value)
{
	__asm__ volatile("outb %0, %1" : : "a"(Value), "Nd"(Port));
}
