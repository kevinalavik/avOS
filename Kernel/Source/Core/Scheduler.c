#include <Core/Scheduler.h>

#include <Arch/Gdt.h>
#include <Core/Log.h>
#include <Memory/Heap.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Vmm.h>

#include <stddef.h>
#include <stdint.h>

static uint8_t FxTemplate[512] __attribute__((aligned(16)));
static bool FxTemplateReady;
#include <string.h>

#define KernelCodeSelector 0x08u
#define KernelDataSelector 0x10u
#define UserCodeSelector 0x1bu
#define UserDataSelector 0x23u

typedef struct SchedulerIretFrame {
	Frame Frame;
	uint64_t rsp;
	uint64_t ss;
} SchedulerIretFrame;

extern void SchedulerEnterFrame(Frame *Frame);
extern void SchedulerLaunch(Process *Proc, uint64_t StackTop);

static Process *ProcessHead;
static Process *ProcessTail;
static Process *CurrentProcess;
static Process *IdleProcess;
static uint64_t NextProcessId = 1;
static bool SchedulerRunning;
static SchedulerIretFrame SchedulerLaunchFrame;

static uint64_t SchedulerReadCr0(void)
{
	uint64_t Value;
	__asm__ volatile("mov %%cr0, %0" : "=r"(Value));
	return Value;
}

static uint64_t SchedulerReadCr2(void)
{
	uint64_t Value;
	__asm__ volatile("mov %%cr2, %0" : "=r"(Value));
	return Value;
}

static uint64_t SchedulerReadCr3(void)
{
	uint64_t Value;
	__asm__ volatile("mov %%cr3, %0" : "=r"(Value));
	return Value;
}

static uint64_t SchedulerReadCr4(void)
{
	uint64_t Value;
	__asm__ volatile("mov %%cr4, %0" : "=r"(Value));
	return Value;
}

static Frame *SchedulerPrepareLaunchFrame(const Frame *CpuFrame, Process *Proc)
{
	SchedulerLaunchFrame.Frame.es =
		CpuFrame != 0 ? CpuFrame->es : KernelDataSelector;
	SchedulerLaunchFrame.Frame.ds =
		CpuFrame != 0 ? CpuFrame->ds : KernelDataSelector;
	SchedulerLaunchFrame.Frame.cr0 =
		CpuFrame != 0 ? CpuFrame->cr0 : SchedulerReadCr0();
	SchedulerLaunchFrame.Frame.cr2 =
		CpuFrame != 0 ? CpuFrame->cr2 : SchedulerReadCr2();
	SchedulerLaunchFrame.Frame.cr3 = Proc->Pml4;
	SchedulerLaunchFrame.Frame.cr4 =
		CpuFrame != 0 ? CpuFrame->cr4 : SchedulerReadCr4();
	SchedulerLaunchFrame.Frame.rax = 0;
	SchedulerLaunchFrame.Frame.rbx = 0;
	SchedulerLaunchFrame.Frame.rcx = 0;
	SchedulerLaunchFrame.Frame.rdx = 0;
	SchedulerLaunchFrame.Frame.rbp = 0;
	SchedulerLaunchFrame.Frame.rdi = 0;
	SchedulerLaunchFrame.Frame.rsi = 0;
	SchedulerLaunchFrame.Frame.r8 = 0;
	SchedulerLaunchFrame.Frame.r9 = 0;
	SchedulerLaunchFrame.Frame.r10 = 0;
	SchedulerLaunchFrame.Frame.r11 = 0;
	SchedulerLaunchFrame.Frame.r12 = 0;
	SchedulerLaunchFrame.Frame.r13 = 0;
	SchedulerLaunchFrame.Frame.r14 = 0;
	SchedulerLaunchFrame.Frame.r15 = 0;
	SchedulerLaunchFrame.Frame.vector = 0;
	SchedulerLaunchFrame.Frame.err = 0;
	SchedulerLaunchFrame.Frame.rflags = 0x2ull;

	if (Proc->UserMode) {
		SchedulerLaunchFrame.Frame.ds = UserDataSelector;
		SchedulerLaunchFrame.Frame.es = UserDataSelector;
		SchedulerLaunchFrame.Frame.rip = Proc->UserRip;
		SchedulerLaunchFrame.Frame.cs = UserCodeSelector;
		SchedulerLaunchFrame.Frame.rflags |= 0x200ull;
		SchedulerLaunchFrame.rsp = Proc->UserRsp;
		SchedulerLaunchFrame.ss = UserDataSelector;
	} else {
		SchedulerLaunchFrame.Frame.rdi = (uint64_t)(uintptr_t)Proc;
		SchedulerLaunchFrame.Frame.rsi = Proc->StackTop;
		SchedulerLaunchFrame.Frame.rip = (uint64_t)(uintptr_t)SchedulerLaunch;
		SchedulerLaunchFrame.Frame.cs = KernelCodeSelector;
		SchedulerLaunchFrame.rsp = 0;
		SchedulerLaunchFrame.ss = 0;
	}

	return &SchedulerLaunchFrame.Frame;
}

static void SchedulerRefreshBlockedWaiters(void)
{
	for (Process *Proc = ProcessHead; Proc != 0; Proc = Proc->Next) {
		if (Proc->State != ProcessStateBlocked || Proc->WaitTarget == 0) {
			continue;
		}

		if (Proc->WaitTarget->State == ProcessStateTerminated) {
			if (Proc->KernelRsp != 0) {
				((Frame *)(uintptr_t)Proc->KernelRsp)->rax =
					(uint64_t)(uint32_t)Proc->WaitTarget->ExitCode;
			}
			Proc->WaitTarget = 0;
			Proc->State = ProcessStateReady;
			Proc->RemainingTicks = Proc->QuantumTicks;
		}
	}
}

static bool SchedulerValidVisitor(SchedulerProcessVisitor Visitor)
{
	return Visitor != 0;
}

static void ProcessCopyName(char Destination[ProcessNameMax], const char *Name)
{
	size_t Index = 0;
	const char *Source = Name != 0 ? Name : "kernel";

	for (; Source[Index] != '\0' && Index + 1 < ProcessNameMax; ++Index) {
		Destination[Index] = Source[Index];
	}

	Destination[Index] = '\0';
}

static void ProcessInsert(Process *Proc)
{
	Proc->Prev = ProcessTail;
	Proc->Next = 0;

	if (ProcessTail != 0) {
		ProcessTail->Next = Proc;
	} else {
		ProcessHead = Proc;
	}

	ProcessTail = Proc;
}

static void SchedulerCleanupProcess(Process *Proc)
{
	if (Proc == 0) {
		return;
	}

	for (uint32_t Index = 0; Index < ProcessMaxFiles; ++Index) {
		if (Proc->FileUsed[Index]) {
			FileClose(&Proc->Files[Index]);
			Proc->FileUsed[Index] = false;
		}
	}

	for (uint32_t Index = 0; Index < ProcessMaxDirectories; ++Index) {
		if (Proc->DirectoryUsed[Index]) {
			DirClose(&Proc->Directories[Index]);
			Proc->DirectoryUsed[Index] = false;
		}
	}

	if (Proc->Cleanup != 0) {
		ProcessCleanup Cleanup = Proc->Cleanup;
		Proc->Cleanup = 0;
		Cleanup(Proc);
	}
}

static void SchedulerIdle(void *Argument)
{
	(void)Argument;

	for (;;) {
		__asm__ volatile("hlt");
	}
}

static void SchedulerExit(void) __attribute__((noreturn));

void SchedulerBootstrap(Process *Proc)
{
	if (Proc == 0 || Proc->Entry == 0) {
		SchedulerExit();
	}

	Proc->Entry(Proc->Argument);
	SchedulerExit();
}

static void SchedulerExit(void)
{
	if (CurrentProcess != 0) {
		SchedulerCleanupProcess(CurrentProcess);
		CurrentProcess->State = ProcessStateTerminated;
	}

	for (;;) {
		__asm__ volatile("sti; hlt");
	}
}

static bool SchedulerProcessRunnable(const Process *Proc)
{
	return Proc != 0 && (Proc->State == ProcessStateReady ||
						 Proc->State == ProcessStateRunning);
}

static Process *SchedulerSelectNext(void)
{
	Process *Start = CurrentProcess != 0 ? CurrentProcess->Next : ProcessHead;
	Process *Candidate = Start;
	Process *IdleCandidate =
		SchedulerProcessRunnable(IdleProcess) ? IdleProcess : 0;

	if (Candidate == 0) {
		Candidate = ProcessHead;
		Start = Candidate;
	}

	if (Candidate == 0) {
		return IdleCandidate;
	}

	do {
		if (SchedulerProcessRunnable(Candidate) && Candidate != IdleProcess) {
			return Candidate;
		}

		Candidate = Candidate->Next;
		if (Candidate == 0) {
			Candidate = ProcessHead;
		}
	} while (Candidate != Start);

	if (SchedulerProcessRunnable(CurrentProcess) &&
		CurrentProcess != IdleProcess) {
		return CurrentProcess;
	}

	return IdleCandidate;
}

bool SchedulerInit(void)
{
	ProcessHead = 0;
	ProcessTail = 0;
	CurrentProcess = 0;
	IdleProcess = 0;
	SchedulerRunning = false;
	NextProcessId = 1;

	__asm__ volatile("fninit");
	__asm__ volatile("fxsave %0" : "=m"(FxTemplate));
	*(uint32_t *)&FxTemplate[24] = 0x1F80u;
	FxTemplateReady = true;

	IdleProcess = SchedulerCreateKernelProcess("idle", SchedulerIdle, 0,
											   ProcessDefaultStackPages);
	if (IdleProcess == 0) {
		return false;
	}

	LogOk("core.sched", "scheduler initialized");
	return true;
}

Process *SchedulerCreateKernelProcess(const char *Name, ProcessEntry Entry,
									  void *Argument, uint64_t StackPages)
{
	if (Entry == 0) {
		return 0;
	}

	if (StackPages == 0) {
		StackPages = ProcessDefaultStackPages;
	}

	Process *Proc = KernelZalloc(sizeof(*Proc));
	if (Proc == 0) {
		return 0;
	}

	uint64_t StackSize = StackPages * PageSize;
	uint64_t StackBase = VmmMapAnonymous(
		StackSize, VmmRegionWritable | VmmRegionAnonymous, PagingFlagWritable);
	if (StackBase == 0) {
		KernelFree(Proc);
		return 0;
	}

	Proc->Id = NextProcessId++;
	Proc->ParentId = 0;
	Proc->ConsoleTargetId = 0;
	Proc->ConsoleInputHead = 0;
	Proc->ConsoleInputTail = 0;
	ProcessCopyName(Proc->Name, Name);
	Proc->State = ProcessStateReady;
	Proc->QuantumTicks = ProcessDefaultQuantum;
	Proc->RemainingTicks = ProcessDefaultQuantum;
	Proc->StackBase = StackBase;
	Proc->StackSize = StackSize;
	Proc->StackTop = StackBase + StackSize;
	Proc->Pml4 = PagingGetRootTablePhys();
	Proc->KernelRsp = 0;
	Proc->Started = false;
	Proc->UserMode = false;
	Proc->ExitCode = ProcessExitCodeNone;
	Proc->Entry = Entry;
	Proc->Cleanup = 0;
	Proc->Argument = Argument;
	Proc->WaitTarget = 0;
	Proc->PulseHead = 0;
	Proc->PulseTail = 0;
	for (size_t i = 0; i < 512; i++)
		Proc->FxState[i] = FxTemplate[i];
	ProcessInsert(Proc);

	LogDebug(
		"core.sched", "created process #%llu '%s' stack=0x%llx pml4=0x%llx",
		(unsigned long long)Proc->Id, Proc->Name,
		(unsigned long long)Proc->StackTop, (unsigned long long)Proc->Pml4);
	return Proc;
}

Process *SchedulerCreateUserProcess(const char *Name, uint64_t Entry,
									uint64_t UserStackBase,
									uint64_t UserStackSize,
									uint64_t UserStackTop, void *Argument,
									uint64_t StackPages)
{
	Process *Proc =
		SchedulerCreateKernelProcess(Name, SchedulerIdle, Argument, StackPages);
	if (Proc == 0) {
		return 0;
	}

	Proc->UserMode = true;
	Proc->UserRip = Entry;
	Proc->UserRsp = UserStackTop;
	Proc->UserStackBase = UserStackBase;
	Proc->UserStackSize = UserStackSize;
	Proc->Entry = 0;
	return Proc;
}

void SchedulerStart(void)
{
	Process *Next = SchedulerSelectNext();
	if (Next == 0) {
		LogFatal("core.sched", "scheduler has no runnable process");
		for (;;) {
			__asm__ volatile("cli; hlt");
		}
	}

	SchedulerRunning = true;
	CurrentProcess = Next;
	CurrentProcess->State = ProcessStateRunning;
	CurrentProcess->RemainingTicks = CurrentProcess->QuantumTicks;
	CurrentProcess->Started = true;
	GdtSetKernelStack(CurrentProcess->StackTop);
	LogTrace("core.sched", "starting process #%llu '%s'",
			 (unsigned long long)CurrentProcess->Id, CurrentProcess->Name);

	if (CurrentProcess->UserMode) {
		SchedulerEnterFrame(SchedulerPrepareLaunchFrame(0, CurrentProcess));
		__builtin_unreachable();
	}

	SchedulerLaunch(CurrentProcess, CurrentProcess->StackTop);
	__builtin_unreachable();
}

Frame *SchedulerSchedule(Frame *CpuFrame)
{
	if (!SchedulerRunning || CpuFrame == 0) {
		return CpuFrame;
	}

	if (CurrentProcess == 0) {
		return CpuFrame;
	}

	CurrentProcess->KernelRsp = (uint64_t)(uintptr_t)CpuFrame;
	CurrentProcess->Pml4 = CpuFrame->cr3;
	CurrentProcess->RuntimeTicks++;

	if (CurrentProcess->Started)
		__asm__ volatile("fxsave %0" : "=m"(CurrentProcess->FxState));

	if (CurrentProcess->RemainingTicks > 0) {
		CurrentProcess->RemainingTicks--;
	}

	if (CurrentProcess->State == ProcessStateRunning) {
		CurrentProcess->State = ProcessStateReady;
	}

	SchedulerRefreshBlockedWaiters();

	Process *Next = CurrentProcess;
	if (CurrentProcess->State == ProcessStateTerminated ||
		CurrentProcess->State == ProcessStateBlocked ||
		CurrentProcess->RemainingTicks == 0) {
		Next = SchedulerSelectNext();
	} else {
		Next = CurrentProcess;
	}

	if (Next == 0) {
		Next = CurrentProcess;
	}

	if (Next->State == ProcessStateTerminated) {
		Next = SchedulerSelectNext();
		if (Next == 0) {
			Next = CurrentProcess;
		}
	}

	Process *Previous = CurrentProcess;
	CurrentProcess = Next;
	CurrentProcess->State = ProcessStateRunning;
	if (CurrentProcess->RemainingTicks == 0) {
		CurrentProcess->RemainingTicks = CurrentProcess->QuantumTicks;
	}

	if (CurrentProcess != Previous) {
		LogTrace("core.sched", "switch #%llu '%s' -> #%llu '%s'",
				 (unsigned long long)Previous->Id, Previous->Name,
				 (unsigned long long)CurrentProcess->Id, CurrentProcess->Name);
	}

	GdtSetKernelStack(CurrentProcess->StackTop);
	if (CurrentProcess->Started)
		__asm__ volatile("fxrstor %0" : : "m"(CurrentProcess->FxState));
	if (!CurrentProcess->Started) {
		CurrentProcess->Started = true;
		return SchedulerPrepareLaunchFrame(CpuFrame, CurrentProcess);
	}

	return (Frame *)(uintptr_t)CurrentProcess->KernelRsp;
}

Process *SchedulerCurrent(void)
{
	return CurrentProcess;
}

Process *SchedulerFindProcess(uint64_t Id)
{
	for (Process *Proc = ProcessHead; Proc != 0; Proc = Proc->Next) {
		if (Proc->Id == Id) {
			return Proc;
		}
	}

	return 0;
}

void SchedulerWaitUntilDone(Process *Target)
{
	if (Target == 0 || CurrentProcess == 0 || Target == CurrentProcess) {
		return;
	}

	if (Target->State == ProcessStateTerminated) {
		return;
	}

	CurrentProcess->WaitTarget = Target;
	CurrentProcess->State = ProcessStateBlocked;
	SchedulerYield();
}

Frame *SchedulerWaitCurrentUntilDone(Frame *CpuFrame, Process *Target)
{
	if (CpuFrame == 0 || Target == 0 || CurrentProcess == 0 ||
		Target == CurrentProcess) {
		return CpuFrame;
	}

	if (Target->State == ProcessStateTerminated) {
		CpuFrame->rax = (uint64_t)(uint32_t)Target->ExitCode;
		return CpuFrame;
	}

	CurrentProcess->WaitTarget = Target;
	CurrentProcess->State = ProcessStateBlocked;
	return SchedulerSchedule(CpuFrame);
}

Frame *SchedulerTerminateCurrent(Frame *CpuFrame, int ExitCode)
{
	if (CurrentProcess == 0) {
		return CpuFrame;
	}

	CurrentProcess->ExitCode = ExitCode;
	SchedulerCleanupProcess(CurrentProcess);
	CurrentProcess->State = ProcessStateTerminated;
	LogDebug("core.sched", "process #%llu '%s' exited with code %d",
			 (unsigned long long)CurrentProcess->Id, CurrentProcess->Name,
			 ExitCode);
	return SchedulerSchedule(CpuFrame);
}

Frame *SchedulerHandleException(Frame *CpuFrame)
{
	if (CpuFrame == 0 || CurrentProcess == 0) {
		return 0;
	}

	if (CpuFrame->vector != 3 || CurrentProcess->Cleanup == 0) {
		return 0;
	}

	LogInfo("core.sched", "process #%llu '%s' hit breakpoint at 0x%llx",
			(unsigned long long)CurrentProcess->Id, CurrentProcess->Name,
			(unsigned long long)CpuFrame->rip);
	SchedulerCleanupProcess(CurrentProcess);
	CurrentProcess->State = ProcessStateTerminated;
	return SchedulerSchedule(CpuFrame);
}

const char *SchedulerStateName(ProcessState State)
{
	switch (State) {
	case ProcessStateNew:
		return "new";
	case ProcessStateReady:
		return "ready";
	case ProcessStateRunning:
		return "running";
	case ProcessStateBlocked:
		return "blocked";
	case ProcessStateTerminated:
		return "dead";
	default:
		return "unknown";
	}
}

uint64_t SchedulerGetProcessCount(void)
{
	uint64_t Count = 0;

	for (Process *Proc = ProcessHead; Proc != 0; Proc = Proc->Next) {
		++Count;
	}

	return Count;
}

bool SchedulerStarted(void)
{
	return SchedulerRunning;
}

void SchedulerVisitProcesses(SchedulerProcessVisitor Visitor, void *Context)
{
	if (!SchedulerValidVisitor(Visitor)) {
		return;
	}

	for (Process *Proc = ProcessHead; Proc != 0; Proc = Proc->Next) {
		Visitor(Proc, Context);
	}
}

void SchedulerYield(void)
{
	if (!SchedulerRunning) {
		return;
	}

	__asm__ volatile("int $32");
}

bool SchedulerPulseSend(Process *Target, uint64_t SenderId, uint32_t Type,
						const void *Payload, uint32_t Size)
{
	uint32_t NextTail;
	ProcessPulse *Pulse;

	if (Target == 0 || Size > ProcessPulsePayloadMax) {
		return false;
	}

	NextTail = (Target->PulseTail + 1u) % ProcessPulseQueueMax;
	if (NextTail == Target->PulseHead) {
		return false;
	}

	Pulse = &Target->Pulses[Target->PulseTail];
	Pulse->SenderId = SenderId;
	Pulse->Type = Type;
	Pulse->Size = Size;

	for (uint32_t Index = 0; Index < Size; ++Index) {
		Pulse->Payload[Index] = ((const uint8_t *)Payload)[Index];
	}
	for (uint32_t Index = Size; Index < ProcessPulsePayloadMax; ++Index) {
		Pulse->Payload[Index] = 0;
	}

	Target->PulseTail = NextTail;
	return true;
}

bool SchedulerPulseReceive(Process *Proc, ProcessPulse *Out)
{
	if (Proc == 0 || Out == 0 || Proc->PulseHead == Proc->PulseTail) {
		return false;
	}

	*Out = Proc->Pulses[Proc->PulseHead];
	Proc->PulseHead = (Proc->PulseHead + 1u) % ProcessPulseQueueMax;
	return true;
}
