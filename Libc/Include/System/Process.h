#ifndef SYSTEM_PROCESS_H
#define SYSTEM_PROCESS_H

#include <System/Types.h>

#define ProcessExitCodeNone (-1)

Handle ProcessSpawn(const char *Path);
U64 ProcessWait(Handle Process);
S64 ProcessPollExit(Handle Process);
Bool ProcessWriteConsoleInput(ProcessId Process, const char *Buffer,
							  Size Length);
ProcessId ProcessCurrentId(void);
ProcessId ProcessParentId(void);
void Exec(const char *Path) __attribute__((noreturn));
void Exit(U64 Code) __attribute__((noreturn));

#endif
