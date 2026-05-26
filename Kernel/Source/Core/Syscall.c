#include <Core/Syscall.h>
#include <Core/Elf.h>
#include <Core/Log.h>
#include <Core/Scheduler.h>
#include <Drivers/Device.h>
#include <Device/Pit.h>
#include <Library/Stdout.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Pmm.h>
#include <Memory/Vmm.h>
#include <stddef.h>

#define UserStringMax 256u
#define ConsolePulseWriteType 0x434F4E31u
#define ConsolePulseTextMax 220u

typedef struct ConsolePulseWrite {
	uint32_t Length;
	char Text[ConsolePulseTextMax];
} ConsolePulseWrite;

static bool ConsoleNameEquals(const char Name[128])
{
	return Name[0] == 'c' && Name[1] == 'o' && Name[2] == 'n' &&
		   Name[3] == 's' && Name[4] == 'o' && Name[5] == 'l' &&
		   Name[6] == 'e' && Name[7] == '\0';
}

static bool ConsoleInputPush(Process *Target, const char *Buffer, size_t Length)
{
	if (Target == 0 || Buffer == 0) {
		return false;
	}

	for (size_t Index = 0; Index < Length; ++Index) {
		uint32_t NextTail =
			(Target->ConsoleInputTail + 1u) % ProcessConsoleInputMax;
		if (NextTail == Target->ConsoleInputHead) {
			return false;
		}

		Target->ConsoleInput[Target->ConsoleInputTail] = (uint8_t)Buffer[Index];
		Target->ConsoleInputTail = NextTail;
	}

	return true;
}

static uint64_t ConsoleInputRead(Process *Current, char *Buffer, size_t Length)
{
	uint64_t Total = 0;

	if (Current == 0 || Buffer == 0 || Length == 0) {
		return (uint64_t)-1;
	}

	while (Total == 0) {
		while (Total < Length &&
			   Current->ConsoleInputHead != Current->ConsoleInputTail) {
			Buffer[Total++] =
				(char)Current->ConsoleInput[Current->ConsoleInputHead];
			Current->ConsoleInputHead =
				(Current->ConsoleInputHead + 1u) % ProcessConsoleInputMax;
		}

		if (Total > 0) {
			return Total;
		}

		__asm__ volatile("sti; hlt; cli" ::: "memory");
	}

	return Total;
}

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static int64_t HandleToIndex(uint64_t Handle, uint64_t Limit)
{
	if (Handle == 0 || Handle > Limit) {
		return -1;
	}

	return (int64_t)(Handle - 1);
}

static void CopyName(char Destination[128], const char *Source)
{
	uint64_t Index = 0;

	if (Destination == 0) {
		return;
	}

	if (Source == 0) {
		Destination[0] = '\0';
		return;
	}

	while (Index + 1 < 128 && Source[Index] != '\0') {
		Destination[Index] = Source[Index];
		++Index;
	}

	Destination[Index] = '\0';
}

static bool UserRangeAccessible(const void *UserPointer, size_t Length)
{
	uint64_t Address = (uint64_t)(uintptr_t)UserPointer;
	uint64_t Start;
	uint64_t End;

	if (Address == 0) {
		return false;
	}
	if (Length == 0) {
		return true;
	}
	if (Address >= 0x0000700000000000ull ||
		Address > UINT64_MAX - (uint64_t)Length) {
		return false;
	}

	Start = AlignDown(Address, PageSize);
	End = AlignDown(Address + Length - 1u, PageSize);
	for (uint64_t Cursor = Start;; Cursor += PageSize) {
		uint64_t Flags = PagingGetFlags(Cursor);
		if ((Flags & (PagingFlagPresent | PagingFlagUser)) !=
			(PagingFlagPresent | PagingFlagUser)) {
			return false;
		}
		if (Cursor == End) {
			break;
		}
	}

	return true;
}

static bool CopyFromUser(void *Destination, const void *UserSource,
						 size_t Length)
{
	if (Destination == 0 || UserSource == 0 ||
		!UserRangeAccessible(UserSource, Length)) {
		return false;
	}

	for (size_t Index = 0; Index < Length; ++Index) {
		((uint8_t *)Destination)[Index] = ((const uint8_t *)UserSource)[Index];
	}

	return true;
}

static bool CopyToUser(void *UserDestination, const void *Source, size_t Length)
{
	if (UserDestination == 0 || Source == 0 ||
		!UserRangeAccessible(UserDestination, Length)) {
		return false;
	}

	for (size_t Index = 0; Index < Length; ++Index) {
		((uint8_t *)UserDestination)[Index] = ((const uint8_t *)Source)[Index];
	}

	return true;
}

static bool CopyUserString(char *Destination, size_t DestinationSize,
						   const char *UserSource)
{
	size_t Index = 0;

	if (Destination == 0 || DestinationSize == 0 || UserSource == 0) {
		return false;
	}

	while (Index + 1 < DestinationSize) {
		if (!UserRangeAccessible(UserSource + Index, 1)) {
			Destination[0] = '\0';
			return false;
		}

		Destination[Index] = UserSource[Index];
		if (Destination[Index] == '\0') {
			return true;
		}
		++Index;
	}

	Destination[DestinationSize - 1] = '\0';
	return false;
}

static uint64_t SyscallDeviceWriteName(const char *UserName, const char *Buffer,
									   size_t Length)
{
	char Name[128];
	Process *Current = SchedulerCurrent();

	if (Buffer == 0) {
		return (uint64_t)-1;
	}

	if (!CopyUserString(Name, sizeof(Name), UserName)) {
		return (uint64_t)-1;
	}
	if (Name[0] == '\0') {
		return (uint64_t)-1;
	}

	if (Current != 0 && Current->ConsoleTargetId != 0 &&
		ConsoleNameEquals(Name)) {
		Process *Target = SchedulerFindProcess(Current->ConsoleTargetId);
		size_t Total = 0;

		if (Target != 0) {
			while (Total < Length) {
				ConsolePulseWrite Pulse;
				size_t Chunk = Length - Total;
				if (Chunk > ConsolePulseTextMax)
					Chunk = ConsolePulseTextMax;

				Pulse.Length = (uint32_t)Chunk;
				if (!CopyFromUser(Pulse.Text, Buffer + Total, Chunk))
					return (uint64_t)-1;
				if (!SchedulerPulseSend(Target, Current->Id,
										ConsolePulseWriteType, &Pulse,
										sizeof(Pulse)))
					break;
				Total += Chunk;
			}

			return (uint64_t)Total;
		}
	}

	Device *Dev = DeviceGet(Name);
	if (Dev != 0) {
		int64_t Written = DeviceWrite(Dev, Buffer, Length);
		return Written >= 0 ? (uint64_t)Written : (uint64_t)-1;
	}

	StdoutLock();
	for (size_t Index = 0; Index < Length; ++Index) {
		char Character;
		if (!CopyFromUser(&Character, Buffer + Index, 1))
			break;
		if (StdoutPutc != 0) {
			StdoutPutc(Character);
			if (Character == '\n')
				StdoutPutc('\r');
		}
	}
	StdoutUnlock();

	return (uint64_t)Length;
}

static uint64_t SyscallDeviceReadName(const char *UserName, char *Buffer,
									  size_t Length)
{
	char Name[128];
	Process *Current = SchedulerCurrent();

	if (Buffer == 0 || Length == 0) {
		return 0;
	}

	if (!CopyUserString(Name, sizeof(Name), UserName)) {
		return (uint64_t)-1;
	}
	if (Name[0] == '\0') {
		return (uint64_t)-1;
	}

	if (Current != 0 && Current->ConsoleTargetId != 0 &&
		ConsoleNameEquals(Name)) {
		return ConsoleInputRead(Current, Buffer, Length);
	}

	Device *Dev = DeviceGet(Name);
	if (Dev == 0) {
		return (uint64_t)-1;
	}

	int64_t Read = DeviceRead(Dev, Buffer, Length);
	return Read >= 0 ? (uint64_t)Read : (uint64_t)-1;
}

static uint64_t SyscallDeviceControlName(const char *UserName, uint64_t Command,
										 void *Argument)
{
	char Name[128];

	if (!CopyUserString(Name, sizeof(Name), UserName)) {
		return (uint64_t)-1;
	}
	if (Name[0] == '\0') {
		return (uint64_t)-1;
	}

	Device *Dev = DeviceGet(Name);
	if (Dev == 0) {
		return (uint64_t)-1;
	}

	int64_t Result = DeviceControl(Dev, Command, Argument);
	return Result >= 0 ? (uint64_t)Result : (uint64_t)-1;
}

static uint64_t SyscallFileOpenPath(const char *Path)
{
	Process *Current = SchedulerCurrent();
	char KernelPath[UserStringMax];
	if (Current == 0 || Path == 0 ||
		!CopyUserString(KernelPath, sizeof(KernelPath), Path)) {
		return (uint64_t)-1;
	}

	for (uint32_t Index = 0; Index < ProcessMaxFiles; ++Index) {
		if (Current->FileUsed[Index]) {
			continue;
		}

		if (!FileOpen(KernelPath, &Current->Files[Index])) {
			return (uint64_t)-1;
		}

		Current->FileUsed[Index] = true;
		return (uint64_t)(Index + 1u);
	}

	return (uint64_t)-1;
}

static uint64_t SyscallFileCloseHandle(uint64_t Handle)
{
	Process *Current = SchedulerCurrent();
	int64_t Index = HandleToIndex(Handle, ProcessMaxFiles);
	if (Current == 0 || Index < 0 || !Current->FileUsed[Index]) {
		return (uint64_t)-1;
	}

	FileClose(&Current->Files[Index]);
	Current->FileUsed[Index] = false;
	return 0;
}

static uint64_t SyscallFileReadHandle(uint64_t Handle, void *Buffer,
									  size_t Length)
{
	Process *Current = SchedulerCurrent();
	int64_t Index = HandleToIndex(Handle, ProcessMaxFiles);
	if (Current == 0 || Index < 0 || !Current->FileUsed[Index] || Buffer == 0) {
		return (uint64_t)-1;
	}
	if (!UserRangeAccessible(Buffer, Length)) {
		return (uint64_t)-1;
	}

	return (uint64_t)FileRead(&Current->Files[Index], Buffer, Length);
}

static uint64_t SyscallFileSeekHandle(uint64_t Handle, size_t Position)
{
	Process *Current = SchedulerCurrent();
	int64_t Index = HandleToIndex(Handle, ProcessMaxFiles);
	if (Current == 0 || Index < 0 || !Current->FileUsed[Index]) {
		return (uint64_t)-1;
	}

	return FileSeek(&Current->Files[Index], Position) ? 0 : (uint64_t)-1;
}

static uint64_t SyscallFileSizeHandle(uint64_t Handle)
{
	Process *Current = SchedulerCurrent();
	int64_t Index = HandleToIndex(Handle, ProcessMaxFiles);
	if (Current == 0 || Index < 0 || !Current->FileUsed[Index]) {
		return (uint64_t)-1;
	}

	return FileSize(&Current->Files[Index]);
}

static uint64_t SyscallDirOpenPath(const char *Path)
{
	Process *Current = SchedulerCurrent();
	char KernelPath[UserStringMax];
	if (Current == 0 || Path == 0 ||
		!CopyUserString(KernelPath, sizeof(KernelPath), Path)) {
		return (uint64_t)-1;
	}

	for (uint32_t Index = 0; Index < ProcessMaxDirectories; ++Index) {
		if (Current->DirectoryUsed[Index]) {
			continue;
		}

		if (!DirOpen(KernelPath, &Current->Directories[Index])) {
			return (uint64_t)-1;
		}

		Current->DirectoryUsed[Index] = true;
		return (uint64_t)(Index + 1u);
	}

	return (uint64_t)-1;
}

static uint64_t SyscallDirCloseHandle(uint64_t Handle)
{
	Process *Current = SchedulerCurrent();
	int64_t Index = HandleToIndex(Handle, ProcessMaxDirectories);
	if (Current == 0 || Index < 0 || !Current->DirectoryUsed[Index]) {
		return (uint64_t)-1;
	}

	DirClose(&Current->Directories[Index]);
	Current->DirectoryUsed[Index] = false;
	return 0;
}

static uint64_t SyscallDirReadHandle(uint64_t Handle, SyscallDirEntry *Out)
{
	Process *Current = SchedulerCurrent();
	VfsDirent Entry;
	int64_t Index = HandleToIndex(Handle, ProcessMaxDirectories);
	if (Current == 0 || Index < 0 || !Current->DirectoryUsed[Index] ||
		Out == 0) {
		return (uint64_t)-1;
	}

	if (!DirRead(&Current->Directories[Index], &Entry)) {
		return 0;
	}

	SyscallDirEntry LocalOut;
	CopyName(LocalOut.Name, Entry.Name);
	LocalOut.Size = Entry.Stat.Size;
	LocalOut.Flags = Entry.Stat.Flags;
	if (!CopyToUser(Out, &LocalOut, sizeof(LocalOut))) {
		return (uint64_t)-1;
	}
	return 1;
}

static uint64_t SyscallProcessSpawnPath(const char *Path)
{
	Process *Child;
	Process *Current = SchedulerCurrent();
	char KernelPath[UserStringMax];

	if (Path == 0 || !CopyUserString(KernelPath, sizeof(KernelPath), Path)) {
		return (uint64_t)-1;
	}

	Child = ElfSpawn(KernelPath);
	if (Child == 0) {
		return (uint64_t)-1;
	}
	if (Current != 0) {
		Child->ParentId = Current->Id;
		Child->ConsoleTargetId = Current->ConsoleTargetId;
	}

	return Child->Id;
}

static uint64_t SyscallPulseSendTo(uint64_t TargetId, uint32_t Type,
								   const void *UserPayload, uint32_t Size)
{
	Process *Current = SchedulerCurrent();
	Process *Target = SchedulerFindProcess(TargetId);
	uint8_t Payload[ProcessPulsePayloadMax];

	if (Current == 0 || Target == 0 || Size > ProcessPulsePayloadMax) {
		return (uint64_t)-1;
	}

	if (Size > 0 &&
		(UserPayload == 0 || !CopyFromUser(Payload, UserPayload, Size))) {
		return (uint64_t)-1;
	}

	if (!SchedulerPulseSend(Target, Current->Id, Type, Payload, Size)) {
		return (uint64_t)-1;
	}

	return 0;
}

static uint64_t SyscallPulseReceiveOut(SyscallPulse *UserOut)
{
	Process *Current = SchedulerCurrent();
	ProcessPulse Pulse;
	SyscallPulse Out;

	if (Current == 0 || UserOut == 0) {
		return (uint64_t)-1;
	}

	if (!SchedulerPulseReceive(Current, &Pulse)) {
		return 0;
	}

	Out.SenderId = Pulse.SenderId;
	Out.Type = Pulse.Type;
	Out.Size = Pulse.Size;
	for (uint32_t Index = 0; Index < Pulse.Size; ++Index) {
		Out.Payload[Index] = Pulse.Payload[Index];
	}
	for (uint32_t Index = Pulse.Size; Index < ProcessPulsePayloadMax; ++Index) {
		Out.Payload[Index] = 0;
	}

	if (!CopyToUser(UserOut, &Out, sizeof(Out))) {
		return (uint64_t)-1;
	}

	return 1;
}

static uint64_t SyscallProcessWriteConsoleInputTo(uint64_t TargetId,
												  const char *UserBuffer,
												  uint32_t Size)
{
	Process *Target = SchedulerFindProcess(TargetId);
	char Buffer[ProcessConsoleInputMax];

	if (Target == 0 || UserBuffer == 0 || Size == 0 ||
		Size > ProcessConsoleInputMax) {
		return (uint64_t)-1;
	}
	if (!CopyFromUser(Buffer, UserBuffer, Size)) {
		return (uint64_t)-1;
	}
	if (!ConsoleInputPush(Target, Buffer, Size)) {
		return (uint64_t)-1;
	}
	return 0;
}

static uint64_t SyscallMapMemory(size_t Size, uint64_t Flags)
{
	uint64_t ActualSize = AlignUp(Size, PageSize);
	if (ActualSize == 0 || ActualSize > 0x00006FFFFFFFFFFFull) {
		return (uint64_t)-1;
	}

	uint64_t Base = VmmReserveRegionInRange(
		0x0000000001000000ull, 0x00006FFFFFFFFFFFull, ActualSize,
		VmmRegionAnonymous | VmmRegionWritable);
	if (Base == 0) {
		return (uint64_t)-1;
	}

	uint64_t PagingFlags = PagingFlagUser;
	if ((Flags & 1u) != 0) {
		PagingFlags |= PagingFlagWritable;
	}

	if (!VmmCommitRange(Base, ActualSize, PagingFlags)) {
		VmmFreeRegion(Base);
		return (uint64_t)-1;
	}

	return Base;
}

#define MAX_SHARED_MEM 16u

typedef struct {
	uint64_t PhysicalBase;
	uint64_t Size;
	bool Used;
} SharedMemEntry;

static SharedMemEntry SharedMemTable[MAX_SHARED_MEM];

static uint64_t SyscallCreateSharedMem(size_t Size)
{
	uint64_t ActualSize = AlignUp(Size, PageSize);
	if (ActualSize == 0 || ActualSize > 0x1000000ull)
		return (uint64_t)-1;

	uint64_t PhysBase = PmmAllocPagesRawPhys(ActualSize / PageSize);
	if (PhysBase == 0)
		return (uint64_t)-1;

	for (uint32_t Index = 0; Index < MAX_SHARED_MEM; Index++) {
		if (!SharedMemTable[Index].Used) {
			SharedMemTable[Index].PhysicalBase = PhysBase;
			SharedMemTable[Index].Size = ActualSize;
			SharedMemTable[Index].Used = true;
			return (uint64_t)(Index + 1u);
		}
	}

	PmmFreePagesPhys(PhysBase, ActualSize / PageSize);
	return (uint64_t)-1;
}

static uint64_t SyscallMapSharedMem(uint64_t Id)
{
	if (Id == 0 || Id > MAX_SHARED_MEM)
		return (uint64_t)-1;

	SharedMemEntry *Entry = &SharedMemTable[Id - 1u];
	if (!Entry->Used)
		return (uint64_t)-1;

	uint64_t VirtualBase = VmmReserveRegionInRange(
		0x0000000001000000ull, 0x00006FFFFFFFFFFFull, Entry->Size,
		VmmRegionMapped | VmmRegionWritable);
	if (VirtualBase == 0)
		return (uint64_t)-1;

	uint64_t PagingFlags =
		PagingFlagPresent | PagingFlagUser | PagingFlagWritable;
	if (!PagingMapRange(VirtualBase, Entry->PhysicalBase, Entry->Size,
						PagingFlags)) {
		VmmFreeRegion(VirtualBase);
		return (uint64_t)-1;
	}

	VmmRegion *Region = (VmmRegion *)VmmFindRegion(VirtualBase);
	if (Region != 0)
		Region->Committed = AlignUp(Entry->Size, PageSize);

	return VirtualBase;
}

static uint64_t SyscallUnmapSharedMem(uint64_t Address)
{
	/* Validate the address range — must be a user mapping */
	if (Address < 0x0000000001000000ull || Address >= 0x0000700000000000ull)
		return (uint64_t)-1;

	const VmmRegion *Region = VmmFindRegion(Address);
	if (Region == 0 || Region->Base != Address)
		return (uint64_t)-1;

	/* For a shared-mem mapping the region is VmmRegionMapped (not Anonymous),
	 * so VmmUncommitRange will unmap the pages without freeing physical memory
	 * (the physical pages are owned by the SharedMemTable entry). */
	return VmmFreeRegion(Address) ? 0 : (uint64_t)-1;
}

static uint64_t SyscallDestroySharedMem(uint64_t Id)
{
	if (Id == 0 || Id > MAX_SHARED_MEM)
		return (uint64_t)-1;

	SharedMemEntry *Entry = &SharedMemTable[Id - 1u];
	if (!Entry->Used)
		return (uint64_t)-1;

	PmmFreePagesPhys(Entry->PhysicalBase, Entry->Size / PageSize);
	Entry->PhysicalBase = 0;
	Entry->Size = 0;
	Entry->Used = false;
	return 0;
}

static uint64_t SyscallUnmapMemory(uint64_t Address, size_t Size)
{
	if (Address < 0x0000000001000000ull || Address >= 0x0000700000000000ull) {
		return (uint64_t)-1;
	}

	uint64_t ActualSize = AlignUp(Size, PageSize);
	if (ActualSize == 0) {
		return (uint64_t)-1;
	}

	const VmmRegion *Region = VmmFindRegion(Address);
	if (Region == 0 || Region->Size < ActualSize) {
		return (uint64_t)-1;
	}

	return VmmFreeRegion(Address) ? 0 : (uint64_t)-1;
}

Frame *SyscallHandle(Frame *CpuFrame)
{
	if (CpuFrame == 0) {
		return 0;
	}

	switch ((SyscallNumber)CpuFrame->rax) {
	case SyscallExit:
		return SchedulerTerminateCurrent(CpuFrame, (int)CpuFrame->rdi);
	case SyscallDeviceWrite:
		CpuFrame->rax = SyscallDeviceWriteName(
			(const char *)(uintptr_t)CpuFrame->rdi,
			(const char *)(uintptr_t)CpuFrame->rsi, (size_t)CpuFrame->rdx);
		return CpuFrame;
	case SyscallDeviceRead:
		CpuFrame->rax = SyscallDeviceReadName(
			(const char *)(uintptr_t)CpuFrame->rdi,
			(char *)(uintptr_t)CpuFrame->rsi, (size_t)CpuFrame->rdx);
		return CpuFrame;
	case SyscallDeviceControl:
		CpuFrame->rax = SyscallDeviceControlName(
			(const char *)(uintptr_t)CpuFrame->rdi, CpuFrame->rsi,
			(void *)(uintptr_t)CpuFrame->rdx);
		return CpuFrame;
	case SyscallFileOpen:
		CpuFrame->rax =
			SyscallFileOpenPath((const char *)(uintptr_t)CpuFrame->rdi);
		return CpuFrame;
	case SyscallFileClose:
		CpuFrame->rax = SyscallFileCloseHandle(CpuFrame->rdi);
		return CpuFrame;
	case SyscallFileRead:
		CpuFrame->rax = SyscallFileReadHandle(CpuFrame->rdi,
											  (void *)(uintptr_t)CpuFrame->rsi,
											  (size_t)CpuFrame->rdx);
		return CpuFrame;
	case SyscallFileWrite:
		CpuFrame->rax = (uint64_t)-1;
		return CpuFrame;
	case SyscallFileSeek:
		CpuFrame->rax =
			SyscallFileSeekHandle(CpuFrame->rdi, (size_t)CpuFrame->rsi);
		return CpuFrame;
	case SyscallFileSize:
		CpuFrame->rax = SyscallFileSizeHandle(CpuFrame->rdi);
		return CpuFrame;
	case SyscallDirOpen:
		CpuFrame->rax =
			SyscallDirOpenPath((const char *)(uintptr_t)CpuFrame->rdi);
		return CpuFrame;
	case SyscallDirClose:
		CpuFrame->rax = SyscallDirCloseHandle(CpuFrame->rdi);
		return CpuFrame;
	case SyscallDirRead:
		CpuFrame->rax = SyscallDirReadHandle(
			CpuFrame->rdi, (SyscallDirEntry *)(uintptr_t)CpuFrame->rsi);
		return CpuFrame;
	case SyscallProcessSpawn:
		CpuFrame->rax =
			SyscallProcessSpawnPath((const char *)(uintptr_t)CpuFrame->rdi);
		return CpuFrame;
	case SyscallProcessWait: {
		Process *Target = SchedulerFindProcess(CpuFrame->rdi);
		if (Target == 0) {
			CpuFrame->rax = (uint64_t)-1;
			return CpuFrame;
		}

		return SchedulerWaitCurrentUntilDone(CpuFrame, Target);
	}
	case SyscallMap:
		CpuFrame->rax = SyscallMapMemory((size_t)CpuFrame->rdi, CpuFrame->rsi);
		return CpuFrame;
	case SyscallUnmap:
		CpuFrame->rax =
			SyscallUnmapMemory(CpuFrame->rdi, (size_t)CpuFrame->rsi);
		return CpuFrame;
	case SyscallSharedMemCreate:
		CpuFrame->rax = SyscallCreateSharedMem((size_t)CpuFrame->rdi);
		return CpuFrame;
	case SyscallSharedMemMap:
		CpuFrame->rax = SyscallMapSharedMem(CpuFrame->rdi);
		return CpuFrame;
	case SyscallSharedMemUnmap:
		CpuFrame->rax = SyscallUnmapSharedMem(CpuFrame->rdi);
		return CpuFrame;
	case SyscallSharedMemDestroy:
		CpuFrame->rax = SyscallDestroySharedMem(CpuFrame->rdi);
		return CpuFrame;
	case SyscallExec: {
		char KernelPath[UserStringMax];
		if (!CopyUserString(KernelPath, sizeof(KernelPath),
							(const char *)(uintptr_t)CpuFrame->rdi) ||
			!ElfExec(KernelPath)) {
			CpuFrame->rax = (uint64_t)-1;
			return CpuFrame;
		}
		Process *Cur = SchedulerCurrent();
		CpuFrame->rip = Cur->UserRip;
		*(uint64_t *)((uintptr_t)CpuFrame + sizeof(Frame)) = Cur->UserRsp;
		CpuFrame->rax = 0;
		CpuFrame->rbx = 0;
		CpuFrame->rcx = 0;
		CpuFrame->rdx = 0;
		CpuFrame->rbp = 0;
		CpuFrame->rdi = 0;
		CpuFrame->rsi = 0;
		CpuFrame->r8 = 0;
		CpuFrame->r9 = 0;
		CpuFrame->r10 = 0;
		CpuFrame->r11 = 0;
		CpuFrame->r12 = 0;
		CpuFrame->r13 = 0;
		CpuFrame->r14 = 0;
		CpuFrame->r15 = 0;
		return CpuFrame;
	}
	case SyscallProcessGetId: {
		Process *Cur = SchedulerCurrent();
		CpuFrame->rax = Cur != 0 ? Cur->Id : 0;
		return CpuFrame;
	}
	case SyscallProcessGetParentId: {
		Process *Cur = SchedulerCurrent();
		CpuFrame->rax = Cur != 0 ? Cur->ParentId : 0;
		return CpuFrame;
	}
	case SyscallPulseSend:
		CpuFrame->rax = SyscallPulseSendTo(
			CpuFrame->rdi, (uint32_t)CpuFrame->rsi,
			(const void *)(uintptr_t)CpuFrame->rdx, (uint32_t)CpuFrame->rcx);
		return CpuFrame;
	case SyscallPulseReceive:
		CpuFrame->rax =
			SyscallPulseReceiveOut((SyscallPulse *)(uintptr_t)CpuFrame->rdi);
		return CpuFrame;
	case SyscallTimeGetTicks:
		CpuFrame->rax = PitGetTicks();
		return CpuFrame;
	case SyscallTimeGetFrequency:
		CpuFrame->rax = PitGetFrequencyHz();
		return CpuFrame;
	case SyscallProcessSetConsoleTarget: {
		Process *Cur = SchedulerCurrent();
		if (Cur == 0) {
			CpuFrame->rax = (uint64_t)-1;
			return CpuFrame;
		}
		if (CpuFrame->rdi != 0 && SchedulerFindProcess(CpuFrame->rdi) == 0) {
			CpuFrame->rax = (uint64_t)-1;
			return CpuFrame;
		}
		Cur->ConsoleTargetId = CpuFrame->rdi;
		CpuFrame->rax = 0;
		return CpuFrame;
	}
	case SyscallProcessPollExit: {
		Process *Target = SchedulerFindProcess(CpuFrame->rdi);
		if (Target == 0) {
			CpuFrame->rax = (uint64_t)-1;
			return CpuFrame;
		}
		if (Target->State != ProcessStateTerminated) {
			CpuFrame->rax = (uint64_t)ProcessExitCodeNone;
			return CpuFrame;
		}
		CpuFrame->rax = (uint64_t)(uint32_t)Target->ExitCode;
		return CpuFrame;
	}
	case SyscallProcessWriteConsoleInput:
		CpuFrame->rax = SyscallProcessWriteConsoleInputTo(
			CpuFrame->rdi, (const char *)(uintptr_t)CpuFrame->rsi,
			(uint32_t)CpuFrame->rdx);
		return CpuFrame;
	default:
		LogWarn("core.syscall", "unknown syscall %llu",
				(unsigned long long)CpuFrame->rax);
		CpuFrame->rax = (uint64_t)-1;
		return CpuFrame;
	}
}