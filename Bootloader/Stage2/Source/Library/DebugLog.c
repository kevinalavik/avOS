#include <Library/DebugLog.h>

#include <Library/Printf.h>

#include <stdarg.h>

static char LowerAscii(char Character)
{
	if (Character >= 'A' && Character <= 'Z') {
		return (char)(Character - 'A' + 'a');
	}

	return Character;
}

static void PrintComponent(const char *Component)
{
	if (Component == 0 || Component[0] == '\0') {
		Printf("loader");
		return;
	}

	while (*Component != '\0') {
		PrintChar(LowerAscii(*Component++));
	}
}

static void WriteMessage(const char *Component, const char *Format,
						 va_list Arguments)
{
	Printf("[boot] ");
	PrintComponent(Component);
	Printf(": ");
	VPrintf(Format, Arguments);
	Printf("\n");
}

void BootError(const char *Component, const char *Format, ...)
{
	va_list Arguments;

	va_start(Arguments, Format);
	WriteMessage(Component, Format, Arguments);
	va_end(Arguments);
}

#if BOOTLOADER_DEBUG
void DebugLog(const char *Component, const char *Format, ...)
{
	va_list Arguments;

	va_start(Arguments, Format);
	WriteMessage(Component, Format, Arguments);
	va_end(Arguments);
}
#endif
