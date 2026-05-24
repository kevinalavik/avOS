#ifndef DEV_CONSOLE_H
#define DEV_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#define VgaDefaultForeground ConsoleColorLightGray
#define VgaDefaultBackground ConsoleColorBlack

typedef struct ConsoleDevice {
	void (*Putc)(char Character);
	void (*Write)(const char *Buffer, size_t Length);
	void (*Clear)(void);
} ConsoleDevice;

typedef enum ConsoleColor {
	ConsoleColorBlack = 0,
	ConsoleColorBlue = 1,
	ConsoleColorGreen = 2,
	ConsoleColorCyan = 3,
	ConsoleColorRed = 4,
	ConsoleColorMagenta = 5,
	ConsoleColorBrown = 6,
	ConsoleColorLightGray = 7,
	ConsoleColorDarkGray = 8,
	ConsoleColorLightBlue = 9,
	ConsoleColorLightGreen = 10,
	ConsoleColorLightCyan = 11,
	ConsoleColorLightRed = 12,
	ConsoleColorLightMagenta = 13,
	ConsoleColorYellow = 14,
	ConsoleColorWhite = 15,
} ConsoleColor;

void ConsoleInit(void);
const ConsoleDevice *ConsoleGetDevice(void);

void ConsoleClear(void);
void ConsolePutc(char Character);
void ConsoleWrite(const char *Buffer, size_t Length);
void ConsolePrint(const char *String);
void ConsolePrintln(const char *String);
void ConsoleSetColor(uint8_t Foreground, uint8_t Background);
uint8_t ConsoleGetColor(void);
void ConsoleSetPackedColor(uint8_t Color);

#endif
