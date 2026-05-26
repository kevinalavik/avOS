#include <Architecture/Acpi.h>
#include <Architecture/LongMode.h>
#include <Boot/BootInfo.h>
#include <Device/Console.h>
#include <Device/Disk.h>

#include <Filesystem/Fat32.h>
#include <Filesystem/Vfs.h>
#include <Library/ConfigParser.h>
#include <Library/Framebuffer.h>
#include <Library/DebugLog.h>
#include <Loader/Elf64.h>
#include <Memory/Allocator.h>
#include <Memory/MemoryMap.h>
#include <Memory/Paging.h>

#define BootReservationPageSize 0x1000ull
#define BootloaderLowStackBase 0x00007000ull
#define BootloaderKernelStackBase 0x00080000ull
#define BootloaderKernelStackTop 0x00090000ull

extern uint8_t __stage2_end;

static void CopyString(char *Destination, size_t DestinationSize,
					   const char *Source)
{
	size_t Index = 0;

	if (DestinationSize == 0) {
		return;
	}

	while (Index + 1u < DestinationSize && Source[Index] != '\0') {
		Destination[Index] = Source[Index];
		++Index;
	}

	Destination[Index] = '\0';
}

static uint64_t Min64(uint64_t A, uint64_t B)
{
	return A < B ? A : B;
}

static uint64_t Max64(uint64_t A, uint64_t B)
{
	return A > B ? A : B;
}

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static bool RangeEnd(uint64_t Base, uint64_t Length, uint64_t *EndOut)
{
	if (Length == 0 || Base > UINT64_MAX - Length) {
		return false;
	}

	*EndOut = Base + Length;
	return true;
}

static bool AppendMemoryMapEntry(BootMemoryMapEntry *Entries,
								 uint32_t *EntriesCount, uint64_t Base,
								 uint64_t End, uint32_t Type)
{
	if (End <= Base) {
		return true;
	}

	if (*EntriesCount >= BootMemoryMapMaxEntries) {
		DebugLog("MEM", "dropping memory map entry 0x%08x%08x-0x%08x%08x",
				 (unsigned int)(Base >> 32), (unsigned int)Base,
				 (unsigned int)(End >> 32), (unsigned int)End);
		return false;
	}

	BootMemoryMapEntry *Entry = &Entries[*EntriesCount];
	Entry->Base = Base;
	Entry->Length = End - Base;
	Entry->Type = Type;
	++*EntriesCount;
	return true;
}

static uint32_t CopyMemoryMap(BootInfo *Info, const MemoryMap *Map)
{
	if (Map == 0) {
		return 0;
	}

	uint32_t EntriesCount = 0;

	for (uint32_t Index = 0;
		 Index < Map->EntriesCount && Index < MemoryMapMaxEntries; ++Index) {
		const MemoryMapEntry *Source = &Map->Entries[Index];
		uint64_t End;

		if (!RangeEnd(Source->Base, Source->Length, &End)) {
			continue;
		}

		AppendMemoryMapEntry(Info->MemoryMap, &EntriesCount, Source->Base, End,
							 Source->Type);
	}

	return EntriesCount;
}

static void ReserveMemoryMapRange(BootInfo *Info, uint32_t *EntriesCount,
								  uint64_t Base, uint64_t End, const char *Name)
{
	if (End <= Base || *EntriesCount == 0) {
		return;
	}

	Base = AlignDown(Base, BootReservationPageSize);
	End = AlignUp(End, BootReservationPageSize);

	BootMemoryMapEntry NewEntries[BootMemoryMapMaxEntries];
	uint32_t NewEntriesCount = 0;

	for (uint32_t Index = 0; Index < *EntriesCount; ++Index) {
		const BootMemoryMapEntry *Entry = &Info->MemoryMap[Index];
		uint64_t EntryEnd;

		if (!RangeEnd(Entry->Base, Entry->Length, &EntryEnd)) {
			continue;
		}

		if (Entry->Type != BootMemoryTypeUsable || EntryEnd <= Base ||
			Entry->Base >= End) {
			AppendMemoryMapEntry(NewEntries, &NewEntriesCount, Entry->Base,
								 EntryEnd, Entry->Type);
			continue;
		}

		uint64_t ReservedBase = Max64(Entry->Base, Base);
		uint64_t ReservedEnd = Min64(EntryEnd, End);

		AppendMemoryMapEntry(NewEntries, &NewEntriesCount, Entry->Base,
							 ReservedBase, Entry->Type);
		AppendMemoryMapEntry(NewEntries, &NewEntriesCount, ReservedBase,
							 ReservedEnd, BootMemoryTypeReserved);
		AppendMemoryMapEntry(NewEntries, &NewEntriesCount, ReservedEnd,
							 EntryEnd, Entry->Type);
	}

	for (uint32_t Index = 0; Index < NewEntriesCount; ++Index) {
		Info->MemoryMap[Index] = NewEntries[Index];
	}

	*EntriesCount = NewEntriesCount;
	DebugLog("MEM", "reserved %s: 0x%08x%08x-0x%08x%08x", Name,
			 (unsigned int)(Base >> 32), (unsigned int)Base,
			 (unsigned int)(End >> 32), (unsigned int)End);
}

static void ReserveBootloaderMemory(BootInfo *Info, uint32_t *EntriesCount)
{
	uintptr_t AllocatedBase;
	uintptr_t AllocatedEnd;
	uint64_t KernelBase;
	uint64_t KernelEnd;

	ReserveMemoryMapRange(Info, EntriesCount, BootloaderLowStackBase,
						  (uint64_t)(uintptr_t)&__stage2_end,
						  "bootloader image");
	ReserveMemoryMapRange(Info, EntriesCount, BootloaderKernelStackBase,
						  BootloaderKernelStackTop, "bootloader stack");
	if (Elf64LoadedRange(&KernelBase, &KernelEnd)) {
		ReserveMemoryMapRange(Info, EntriesCount, KernelBase, KernelEnd,
							  "kernel image");
	}

	if (AllocatorAllocatedRange(&AllocatedBase, &AllocatedEnd)) {
		ReserveMemoryMapRange(Info, EntriesCount, (uint64_t)AllocatedBase,
							  (uint64_t)AllocatedEnd, "bootloader allocs");
	}
}

static BootInfo *CreateBootInfo(const MemoryMap *Map, uint64_t KernelEntry,
								const char *KernelPath, const char *Cmdline,
								const AcpiRootPointers *AcpiRoots)
{
	BootInfo *Info = Alloc(sizeof(BootInfo), 16);
	if (Info == 0) {
		return 0;
	}

	Info->Magic = BootInfoMagic;
	Info->Version = BootInfoVersion;
	Info->Size = sizeof(BootInfo);
	Info->KernelEntry = KernelEntry;
	Info->KernelPhysicalBase = PagingGetKernelPhysicalBase();
	Info->HhdmOffset = PagingGetHhdmOffset();
	Info->AcpiRsdpAddress = AcpiRoots->RsdpAddress;
	CopyString(Info->KernelPath, sizeof(Info->KernelPath), KernelPath);
	CopyString(Info->Cmdline, sizeof(Info->Cmdline), Cmdline);
	Info->MemoryMapEntriesCount = CopyMemoryMap(Info, Map);
	ReserveBootloaderMemory(Info, &Info->MemoryMapEntriesCount);

	Info->Framebuffer.Address = 0;
	Info->Framebuffer.Width = 0;
	Info->Framebuffer.Height = 0;
	Info->Framebuffer.Pitch = 0;
	Info->Framebuffer.Bpp = 0;

	return Info;
}

void S2Entry(void)
{
	ConsoleInit();
	DebugLog("ENTRY", "avOS kernel loader v1.0!");

	const MemoryMap *Map = MemoryMapGetBoot();
	MemoryMapLog(Map);
	if (!AllocatorInit(Map)) {
		BootError("ENTRY", "allocator unavailable");
		goto halt;
	}

	if (!DiskInit()) {
		BootError("BIOS", "disk init failed");
		goto halt;
	}
	DebugLog("BIOS", "INT 13h disk ready");

	Fat32Volume *Volume = Alloc(sizeof(Fat32Volume), 16);
	if (Volume == 0) {
		BootError("FAT32", "failed to allocate volume");
		goto halt;
	}

	if (!Fat32Mount(Volume, DiskGetDevice())) {
		BootError("FAT32", "mount failed");
		goto halt;
	}

	DebugLog("FAT32", "mounted partition LBA %u, root cluster %u",
			 (unsigned int)Volume->PartitionLba,
			 (unsigned int)Volume->RootCluster);

	if (!VfsMountRoot(Fat32GetFilesystemOps(), Volume)) {
		BootError("ENTRY", "failed to mount root");
		goto halt;
	}

	DebugLog("ENTRY", "mounted %s", VfsRootPath());

	char *BootConfig;
	if (!ReadTextFile(BootConfigPath, &BootConfig)) {
		BootError("ENTRY", "failed to read %s", BootConfigPath);
		goto halt;
	}

	char KernelPath[BootPathMax];
	if (!ParseKernelPath(BootConfig, KernelPath)) {
		BootError("ENTRY", "missing kernel in %s", BootConfigPath);
		goto halt;
	}

	bool FramebufferEnabled = ParseFramebufferEnabled(BootConfig);

	char Cmdline[BootCmdlineMax];
	if (!ParseCmdline(BootConfig, Cmdline)) {
		BootError("ENTRY", "cmdline too long in %s", BootConfigPath);
		goto halt;
	}

	uint64_t KernelEntry;
	if (!Elf64Load(KernelPath, &KernelEntry)) {
		BootError("ENTRY", "failed to load kernel ELF '%s'", KernelPath);
		goto halt;
	}

	AcpiRootPointers AcpiRoots = AcpiFindRootPointers();

	BootInfo *Info =
		CreateBootInfo(Map, KernelEntry, KernelPath, Cmdline, &AcpiRoots);
	if (Info == 0) {
		BootError("ENTRY", "failed to allocate boot info");
		goto halt;
	}

	if (FramebufferEnabled) {
		FramebufferInit(Info);
	} else {
		DebugLog("FB", "disabled by config; keeping 80x25 text mode");
	}

	uint32_t PageMap = PagingBuildKernelMap(&Info->Framebuffer);
	DebugLog("ENTRY", "loaded kernel '%s'", KernelPath);

	DebugLog("ENTRY", "entering long mode");
	EnterLongMode(PageMap, KernelEntry, (uint32_t)Info);

halt:
	for (;;) {
		__asm__ volatile("hlt");
	}
}
