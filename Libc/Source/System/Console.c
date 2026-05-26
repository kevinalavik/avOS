#include <System/Console.h>
#include <System/Syscall.h>
#include <Lib/String.h>

Size ConsoleWrite(const char *Buffer, Size Length)
{
	return Syscall3(SyscallDeviceWrite, (U64) "console", (U64)Buffer, Length);
}

Size ConsoleRead(char *Buffer, Size Length)
{
	return Syscall3(SyscallDeviceRead, (U64) "console", (U64)Buffer, Length);
}

Size Print(const char *Text)
{
	return ConsoleWrite(Text, StringLength(Text));
}

Bool ConsoleSetTarget(ProcessId Process)
{
	return Syscall1(SyscallProcessSetConsoleTarget, Process) == 0;
}
