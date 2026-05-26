#ifndef CORE_SCHEDULER_H
#define CORE_SCHEDULER_H

#include <Arch/Idt.h>
#include <Filesystem/Vfs.h>

#include <stdbool.h>
#include <stdint.h>

#define ProcessNameMax 32u
#define ProcessDefaultQuantum 5u
#define ProcessDefaultStackPages 16ull
#define ProcessExitCodeNone (-1)
#define ProcessMaxFiles 16u
#define ProcessMaxDirectories 8u
#define ProcessPulsePayloadMax 256u
#define ProcessPulseQueueMax 32u
#define ProcessConsoleInputMax 256u

typedef enum ProcessState {
	ProcessStateNew = 0,
	ProcessStateReady,
	ProcessStateRunning,
	ProcessStateBlocked,
	ProcessStateTerminated,
} ProcessState;

typedef struct Process Process;
typedef struct ProcessPulse {
	uint64_t SenderId;
	uint32_t Type;
	uint32_t Size;
	uint8_t Payload[ProcessPulsePayloadMax];
} ProcessPulse;

typedef void (*ProcessEntry)(void *Argument);
typedef void (*SchedulerProcessVisitor)(const Process *Proc, void *Context);
typedef void (*ProcessCleanup)(Process *Proc);

struct Process {
	__attribute__((aligned(16))) uint8_t FxState[512];
	uint64_t Id;
	uint64_t ParentId;
	uint64_t ConsoleTargetId;
	uint8_t ConsoleInput[ProcessConsoleInputMax];
	uint32_t ConsoleInputHead;
	uint32_t ConsoleInputTail;
	char Name[ProcessNameMax];
	ProcessState State;
	uint32_t QuantumTicks;
	uint32_t RemainingTicks;
	uint64_t RuntimeTicks;
	uint64_t StackBase;
	uint64_t StackSize;
	uint64_t StackTop;
	uint64_t UserRip;
	uint64_t UserRsp;
	uint64_t UserStackBase;
	uint64_t UserStackSize;
	uint64_t KernelRsp;
	uint64_t Pml4;
	bool Started;
	bool UserMode;
	int ExitCode;
	ProcessEntry Entry;
	ProcessCleanup Cleanup;
	void *Argument;
	struct Process *WaitTarget;
	File Files[ProcessMaxFiles];
	bool FileUsed[ProcessMaxFiles];
	Directory Directories[ProcessMaxDirectories];
	bool DirectoryUsed[ProcessMaxDirectories];
	ProcessPulse Pulses[ProcessPulseQueueMax];
	uint32_t PulseHead;
	uint32_t PulseTail;
	struct Process *Next;
	struct Process *Prev;
};

bool SchedulerInit(void);
Process *SchedulerCreateKernelProcess(const char *Name, ProcessEntry Entry,
									  void *Argument, uint64_t StackPages);
Process *SchedulerCreateUserProcess(const char *Name, uint64_t Entry,
									uint64_t UserStackBase,
									uint64_t UserStackSize,
									uint64_t UserStackTop, void *Argument,
									uint64_t StackPages);
void SchedulerStart(void) __attribute__((noreturn));
Frame *SchedulerSchedule(Frame *Frame);
Process *SchedulerCurrent(void);
Process *SchedulerFindProcess(uint64_t Id);
void SchedulerWaitUntilDone(Process *Target);
Frame *SchedulerWaitCurrentUntilDone(Frame *Frame, Process *Target);
Frame *SchedulerTerminateCurrent(Frame *Frame, int ExitCode);
Frame *SchedulerHandleException(Frame *Frame);
const char *SchedulerStateName(ProcessState State);
uint64_t SchedulerGetProcessCount(void);
bool SchedulerStarted(void);
void SchedulerVisitProcesses(SchedulerProcessVisitor Visitor, void *Context);
void SchedulerYield(void);
bool SchedulerPulseSend(Process *Target, uint64_t SenderId, uint32_t Type,
						const void *Payload, uint32_t Size);
bool SchedulerPulseReceive(Process *Proc, ProcessPulse *Out);

#endif
