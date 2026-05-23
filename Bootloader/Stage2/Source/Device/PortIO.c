#include <Device/PortIO.h>

uint32_t PortIORead(PortIOWidth eWidth, uint16_t Port)
{
	switch (eWidth) {
	case PortIOWidth8: {
		uint8_t Value;
		__asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));
		return Value;
	}
	case PortIOWidth16: {
		uint16_t Value;
		__asm__ volatile("inw %1, %0" : "=a"(Value) : "Nd"(Port));
		return Value;
	}
	case PortIOWidth32: {
		uint32_t Value;
		__asm__ volatile("inl %1, %0" : "=a"(Value) : "Nd"(Port));
		return Value;
	}
	}

	return 0;
}

void PortIOWrite(PortIOWidth eWidth, uint16_t Port, uint32_t Data)
{
	switch (eWidth) {
	case PortIOWidth8:
		__asm__ volatile("outb %0, %1" : : "a"((uint8_t)Data), "Nd"(Port));
		break;
	case PortIOWidth16:
		__asm__ volatile("outw %0, %1" : : "a"((uint16_t)Data), "Nd"(Port));
		break;
	case PortIOWidth32:
		__asm__ volatile("outl %0, %1" : : "a"(Data), "Nd"(Port));
		break;
	}
}
