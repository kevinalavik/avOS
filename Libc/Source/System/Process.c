#include <System/Process.h>
#include <System/Syscall.h>

Handle ProcessSpawn(const char *Path)
{
	return (Handle)Syscall1(SyscallProcessSpawn, (U64)Path);
}

U64 ProcessWait(Handle Process)
{
	return Syscall1(SyscallProcessWait, Process);
}

S64 ProcessPollExit(Handle Process)
{
	return (S64)Syscall1(SyscallProcessPollExit, Process);
}

Bool ProcessWriteConsoleInput(ProcessId Process, const char *Buffer,
							  Size Length)
{
	return Syscall3(SyscallProcessWriteConsoleInput, Process, (U64)Buffer,
					Length) == 0;
}

ProcessId ProcessCurrentId(void)
{
	return (ProcessId)Syscall0(SyscallProcessGetId);
}

ProcessId ProcessParentId(void)
{
	return (ProcessId)Syscall0(SyscallProcessGetParentId);
}

void Exec(const char *Path)
{
	Syscall1(SyscallExec, (U64)Path);
	while (1)
		;
}

void Exit(U64 Code)
{
	Syscall1(SyscallExit, Code);
	while (1)
		;
}
