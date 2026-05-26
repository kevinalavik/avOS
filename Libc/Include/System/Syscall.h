#ifndef SYSTEM_SYSCALL_H
#define SYSTEM_SYSCALL_H

#include <System/Types.h>

typedef enum SyscallNumber {
	SyscallExit = 1,
	SyscallDeviceWrite = 2,
	SyscallDeviceRead = 3,
	SyscallDeviceControl = 4,
	SyscallFileOpen = 5,
	SyscallFileClose = 6,
	SyscallFileRead = 7,
	SyscallFileWrite = 8,
	SyscallFileSeek = 9,
	SyscallFileSize = 10,
	SyscallDirOpen = 11,
	SyscallDirClose = 12,
	SyscallDirRead = 13,
	SyscallProcessSpawn = 14,
	SyscallProcessWait = 15,
	SyscallMap = 16,
	SyscallUnmap = 17,
	SyscallExec = 18,
	SyscallProcessGetId = 19,
	SyscallProcessGetParentId = 20,
	SyscallPulseSend = 21,
	SyscallPulseReceive = 22,
	SyscallTimeGetTicks = 23,
	SyscallTimeGetFrequency = 24,
	SyscallProcessSetConsoleTarget = 25,
	SyscallProcessPollExit = 26,
	SyscallProcessWriteConsoleInput = 27,
	SyscallSharedMemCreate = 28,
	SyscallSharedMemMap = 29,
	SyscallSharedMemUnmap = 30,
	SyscallSharedMemDestroy = 31,
} SyscallNumber;

U64 Syscall0(U64 Number);
U64 Syscall1(U64 Number, U64 Arg0);
U64 Syscall2(U64 Number, U64 Arg0, U64 Arg1);
U64 Syscall3(U64 Number, U64 Arg0, U64 Arg1, U64 Arg2);
U64 Syscall4(U64 Number, U64 Arg0, U64 Arg1, U64 Arg2, U64 Arg3);

#endif