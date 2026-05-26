#ifndef SYSTEM_CONSOLE_H
#define SYSTEM_CONSOLE_H

#include <System/Types.h>

#define CONSOLE_PULSE_WRITE 0x434F4E31u
#define CONSOLE_PULSE_TEXT_MAX 220u

typedef struct {
	U32 Length;
	char Text[CONSOLE_PULSE_TEXT_MAX];
} ConsolePulseWrite;

Size ConsoleWrite(const char *Buffer, Size Length);
Size ConsoleRead(char *Buffer, Size Length);
Size Print(const char *Text);
Bool ConsoleSetTarget(ProcessId Process);

#endif
