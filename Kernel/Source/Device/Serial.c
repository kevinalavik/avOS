#include <Device/Serial.h>
#include <Device/PortIO.h>

#include <stdbool.h>

#define SerialData(Port) (Port)
#define SerialInterrupt(Port) ((Port) + 1)
#define SerialFifoControl(Port) ((Port) + 2)
#define SerialLineControl(Port) ((Port) + 3)
#define SerialModemControl(Port) ((Port) + 4)
#define SerialLineStatus(Port) ((Port) + 5)

#define SerialDlabLow(Port) (Port)
#define SerialDlabHigh(Port) ((Port) + 1)

static uint16_t SerialPort;

void SerialInit(uint16_t Port, uint32_t Baud)
{
	SerialPort = Port;

	uint32_t Divisor = 115200 / Baud;
	if (Divisor == 0) {
		Divisor = 1;
	}

	PortIOWrite8(SerialInterrupt(Port), 0);
	PortIOWrite8(SerialLineControl(Port), 0x80);
	PortIOWrite8(SerialDlabLow(Port), (uint8_t)(Divisor & 0xFF));
	PortIOWrite8(SerialDlabHigh(Port), (uint8_t)((Divisor >> 8) & 0xFF));
	PortIOWrite8(SerialLineControl(Port), 0x03);
	PortIOWrite8(SerialFifoControl(Port), 0xC7);
	PortIOWrite8(SerialModemControl(Port), 0x0B);
	PortIOWrite8(SerialInterrupt(Port), 0x0F);
}

static bool SerialIsTransmitEmpty(void)
{
	return (PortIORead8(SerialLineStatus(SerialPort)) & 0x20) != 0;
}

void SerialPutc(char Character)
{
	while (!SerialIsTransmitEmpty()) {
		__asm__ volatile("pause");
	}

	if (Character == '\n') {
		PortIOWrite8(SerialData(SerialPort), '\r');
		while (!SerialIsTransmitEmpty()) {
			__asm__ volatile("pause");
		}
	}

	PortIOWrite8(SerialData(SerialPort), (uint8_t)Character);
}

void SerialWrite(const char *Buffer, size_t Length)
{
	for (size_t Index = 0; Index < Length; ++Index) {
		SerialPutc(Buffer[Index]);
	}
}

void SerialPrint(const char *String)
{
	while (*String != '\0') {
		SerialPutc(*String++);
	}
}
