#include <Device/Console.h>
#include <Device/PortIO.h>
#include <Device/Serial.h>
#include <Library/Stdout.h>

#define VgaMemory ((volatile uint16_t *)0xB8000)
#define TtyWidth 80

static uint8_t ConsoleColorValue;

static uint8_t VgaColor(uint8_t Foreground, uint8_t Background)
{
	return (uint8_t)((Background << 4) | (Foreground & 0x0F));
}

static uint16_t VgaEntry(char Character, uint8_t Color)
{
	return (uint16_t)(uint8_t)Character | ((uint16_t)Color << 8);
}

static void SetCursor(uint16_t Position)
{
	PortIOWrite(PortIOWidth8, 0x3D4, 0x0F);
	PortIOWrite(PortIOWidth8, 0x3D5, Position & 0xFF);
	PortIOWrite(PortIOWidth8, 0x3D4, 0x0E);
	PortIOWrite(PortIOWidth8, 0x3D5, (Position >> 8) & 0xFF);
}

static uint16_t GetCursor(void)
{
	PortIOWrite(PortIOWidth8, 0x3D4, 0x0F);
	uint16_t Pos = PortIORead(PortIOWidth8, 0x3D5);
	PortIOWrite(PortIOWidth8, 0x3D4, 0x0E);
	Pos |= (uint16_t)PortIORead(PortIOWidth8, 0x3D5) << 8;
	return Pos;
}

void ConsoleSetColor(uint8_t Foreground, uint8_t Background)
{
	ConsoleColorValue = VgaColor(Foreground, Background);
}

uint8_t ConsoleGetColor(void)
{
	return ConsoleColorValue;
}

void ConsoleSetPackedColor(uint8_t Color)
{
	ConsoleColorValue = Color;
}

void ConsoleClear(void)
{
	for (uint16_t i = 0; i < 4000; ++i) {
		VgaMemory[i] = VgaEntry(' ', ConsoleColorValue);
	}

	SetCursor(0);
}

void ConsolePutc(char Character)
{
	SerialPutc(Character);

	uint16_t Pos = GetCursor();

	switch (Character) {
	case '\n':
		Pos = ((Pos / TtyWidth) + 1) * TtyWidth;
		break;
	case '\r':
		Pos = (Pos / TtyWidth) * TtyWidth;
		break;
	case '\t':
		do {
			VgaMemory[Pos] = VgaEntry(' ', ConsoleColorValue);
			++Pos;
		} while (Pos % 4 != 0);
		break;
	case '\b':
		if (Pos > 0) {
			--Pos;
			VgaMemory[Pos] = VgaEntry(' ', ConsoleColorValue);
		}
		break;
	default:
		VgaMemory[Pos] = VgaEntry(Character, ConsoleColorValue);
		++Pos;
		break;
	}

	SetCursor(Pos);
}

void ConsoleWrite(const char *Buffer, size_t Length)
{
	for (size_t Index = 0; Index < Length; ++Index) {
		ConsolePutc(Buffer[Index]);
	}
}

void ConsolePrint(const char *String)
{
	while (*String != '\0') {
		ConsolePutc(*String++);
	}
}

void ConsolePrintln(const char *String)
{
	ConsolePrint(String);
	ConsolePutc('\n');
}

static const ConsoleDevice ConsoleDeviceInstance = {
	.Putc = ConsolePutc,
	.Write = ConsoleWrite,
	.Clear = ConsoleClear,
};

const ConsoleDevice *ConsoleGetDevice(void)
{
	return &ConsoleDeviceInstance;
}

void ConsoleInit(void)
{
	ConsoleSetColor(VgaDefaultForeground, VgaDefaultBackground);
	SerialInit(SerialCom1, 115200);
	StdoutPutc = ConsolePutc;
	ConsoleClear();
}
