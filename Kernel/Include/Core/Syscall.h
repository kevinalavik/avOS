#ifndef CORE_SYSCALL_H
#define CORE_SYSCALL_H

#include <Arch/Idt.h>

#include <stdint.h>

#define SyscallVector 0x80u
#define SyscallPulsePayloadMax 256u

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

typedef struct SyscallDirEntry {
	char Name[128];
	uint64_t Size;
	uint32_t Flags;
} SyscallDirEntry;

typedef struct SyscallPulse {
	uint64_t SenderId;
	uint32_t Type;
	uint32_t Size;
	uint8_t Payload[SyscallPulsePayloadMax];
} SyscallPulse;

Frame *SyscallHandle(Frame *Frame);

#endif
