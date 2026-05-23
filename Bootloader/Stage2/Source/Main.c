#include <Architecture/LongMode.h>
#include <Boot/BootInfo.h>
#include <Device/Console.h>
#include <Device/Disk.h>

#include <Filesystem/Fat32.h>
#include <Filesystem/TmpFs.h>
#include <Filesystem/Vfs.h>
#include <Library/ConfigParser.h>
#include <Library/Framebuffer.h>
#include <Library/Log.h>
#include <Loader/Elf64.h>
#include <Memory/Allocator.h>
#include <Memory/MemoryMap.h>
#include <Memory/Paging.h>

static const char TmpReadme[] = "tmpfs online\n";

static void PrintRootEntry(const Fat32DirectoryEntry *Entry, void *Context)
{
	const char *RootPath = Context;

	LogInfo("FS", "%c %s%s (%u bytes, cluster %u)",
			(Entry->Attributes & Fat32AttrDirectory) != 0 ? 'd' : '-', RootPath,
			Entry->Name, (unsigned int)Entry->Size,
			(unsigned int)Entry->FirstCluster);
}

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

static uint32_t CopyMemoryMap(BootInfo *Info, const MemoryMap *Map)
{
	if (Map == 0) {
		return 0;
	}

	uint32_t EntriesCount = Map->EntriesCount;
	if (EntriesCount > BootMemoryMapMaxEntries) {
		EntriesCount = BootMemoryMapMaxEntries;
	}

	for (uint32_t Index = 0; Index < EntriesCount; ++Index) {
		const MemoryMapEntry *Source = &Map->Entries[Index];
		BootMemoryMapEntry *Destination = &Info->MemoryMap[Index];

		Destination->Base = Source->Base;
		Destination->Length = Source->Length;
		Destination->Type = Source->Type;
	}

	return EntriesCount;
}

static BootInfo *CreateBootInfo(const MemoryMap *Map, uint64_t KernelEntry,
								const char *KernelPath)
{
	BootInfo *Info = Alloc(sizeof(BootInfo), 16);
	if (Info == 0) {
		return 0;
	}

	Info->Magic = BootInfoMagic;
	Info->Version = BootInfoVersion;
	Info->Size = sizeof(BootInfo);
	Info->KernelEntry = KernelEntry;
	CopyString(Info->KernelPath, sizeof(Info->KernelPath), KernelPath);
	Info->MemoryMapEntriesCount = CopyMemoryMap(Info, Map);

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
	LogOk("ENTRY", "avOS stage2 loaded !");

	const MemoryMap *Map = MemoryMapGetBoot();
	MemoryMapLog(Map);
	if (!AllocatorInit(Map)) {
		LogError("ENTRY", "allocator unavailable");
		goto halt;
	}

	if (!DiskInit()) {
		LogError("ATA", "init failed");
		goto halt;
	}
	LogOk("ATA", "ATA disk ready");

	Fat32Volume *Volume = Alloc(sizeof(Fat32Volume), 16);
	if (Volume == 0) {
		LogError("FAT32", "failed to allocate volume");
		goto halt;
	}

	if (!Fat32Mount(Volume, DiskGetDevice())) {
		LogError("FAT32", "mount failed");
		goto halt;
	}

	LogOk("FAT32", "mounted partition LBA %u, root cluster %u",
		  (unsigned int)Volume->PartitionLba,
		  (unsigned int)Volume->RootCluster);

	if (!VfsMountRoot(Fat32GetFilesystemOps(), Volume)) {
		LogError("ENTRY", "failed to mount root");
		goto halt;
	}

	TmpFs *Instance = Alloc(sizeof(TmpFs), 16);
	if (Instance != 0 && TmpFsInit(Instance, TmpFsMaxNodes)) {
		(void)TmpFsAddFile(Instance, "readme.txt", TmpReadme,
						   sizeof(TmpReadme) - 1u);
		(void)VfsMount("tmp", TmpFsGetFilesystemOps(), Instance);
	}

	LogOk("ENTRY", "mounted %s", VfsRootPath());
	LogInfo("ENTRY", "root directory:");
	if (!Fat32ListRoot(Volume, PrintRootEntry, (void *)VfsRootPath())) {
		LogError("ENTRY", "root listing failed");
		goto halt;
	}

	char *BootConfig;
	if (!ReadTextFile(BootConfigPath, &BootConfig)) {
		LogError("ENTRY", "failed to read %s", BootConfigPath);
		goto halt;
	}

	char KernelPath[BootPathMax];
	if (!ParseKernelPath(BootConfig, KernelPath)) {
		LogError("ENTRY", "missing kernel in %s", BootConfigPath);
		goto halt;
	}

	uint64_t KernelEntry;
	if (!Elf64Load(KernelPath, &KernelEntry)) {
		LogError("ENTRY", "failed to load kernel ELF '%s'", KernelPath);
		goto halt;
	}

	BootInfo *Info = CreateBootInfo(Map, KernelEntry, KernelPath);
	if (Info == 0) {
		LogError("ENTRY", "failed to allocate boot info");
		goto halt;
	}

	FramebufferInit(Info);

	uint32_t PageMap = PagingBuildKernelMap(&Info->Framebuffer);
	LogOk("ENTRY", "loaded kernel '%s'", KernelPath);

	LogOk("ENTRY", "entering long mode");
	EnterLongMode(PageMap, KernelEntry, (uint32_t)Info);

halt:
	for (;;) {
		__asm__ volatile("hlt");
	}
}
