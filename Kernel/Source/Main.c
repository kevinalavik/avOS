#include <Acpi/Acpi.h>
#include <Acpi/Madt.h>
#include <Arch/Gdt.h>
#include <Arch/Idt.h>
#include <Arch/Irq.h>
#include <Boot/BootInfo.h>
#include <Core/Elf.h>
#include <Core/Log.h>
#include <Core/Scheduler.h>
#include <Device/Framebuffer.h>
#include <Device/Pit.h>
#include <Device/Serial.h>
#include <Device/TextConsole.h>
#include <Drivers/Device.h>
#include <Drivers/Display/Fb.h>
#include <Drivers/Display/FbConsole.h>
#include <Drivers/Display/TextModeConsole.h>
#include <Drivers/Input/Keyboard.h>
#include <Drivers/Input/Mouse.h>
#include <Drivers/Storage/Disk.h>
#include <Filesystem/Fat32.h>
#include <Filesystem/Vfs.h>
#include <Library/Printf.h>
#include <Library/Stdout.h>
#include <Memory/Heap.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Pmm.h>
#include <Memory/Vmm.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <stddef.h>
#include <stdint.h>

static struct flanterm_context *FtCtx;
static uint64_t BootInfoPhysical;
static char BootVolumeRoot[VfsPathMax] = "a:/";

#define KernelStackPages 16ull

/* flanterm needs these */
void *memset(void *s, int c, size_t n)
{
	for (size_t i = 0; i < n; i++)
		((unsigned char *)s)[i] = (unsigned char)c;
	return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	for (size_t i = 0; i < n; i++)
		((unsigned char *)dest)[i] = ((const unsigned char *)src)[i];
	return dest;
}
/* end-note */

static void LogKernelPutc(char Character)
{
	SerialPutc(Character);
	if (FtCtx != NULL && FbConsoleAvailable()) {
		flanterm_write(FtCtx, &Character, 1);
	} else if (TextConsoleReady()) {
		TextConsolePutc(Character);
	}
}

static void HaltForever(void)
{
	for (;;) {
		__asm__ volatile("hlt");
	}
}

static void FatalBoot(const char *Component, const char *Message)
{
	LogFatal(Component, "%s", Message);
	HaltForever();
}

static uint64_t ReadStackPointer(void)
{
	uint64_t StackPointer;

	__asm__ volatile("movq %%rsp, %0" : "=r"(StackPointer));
	return StackPointer;
}

static void InitFramebufferConsole(const BootFramebuffer *Framebuffer)
{
	FtCtx = NULL;

	if (FramebufferInit(Framebuffer)) {
		LogDebug(
			"core.graphic.fb", "framebuffer: %ux%u pitch=%u bpp=%u addr=0x%llx",
			(unsigned int)Framebuffer->Width, (unsigned int)Framebuffer->Height,
			(unsigned int)Framebuffer->Pitch, (unsigned int)Framebuffer->Bpp,
			(unsigned long long)Framebuffer->Address);

		FtCtx = flanterm_fb_init(
			NULL, NULL, (uint32_t *)(uintptr_t)Framebuffer->Address,
			Framebuffer->Width, Framebuffer->Height, Framebuffer->Pitch, 8, 16,
			8, 8, 8, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1,
			0, 0, 0, 0);

		if (FtCtx != NULL) {
			StdoutPutc = LogKernelPutc;
			FbConsoleInit(FtCtx);
			LogOk("core.graphic.fb", "framebuffer terminal initialized");
		} else {
			LogWarn("core.graphic.fb", "flanterm init returned null");
		}
	} else {
		LogWarn("core.graphic.fb", "framebuffer unavailable");
	}
}

static bool BuildVolumeRoot(char *Destination, size_t DestinationSize,
							char VolumeId)
{
	if (DestinationSize < 4) {
		return false;
	}

	Destination[0] = VolumeId;
	Destination[1] = ':';
	Destination[2] = '/';
	Destination[3] = '\0';
	return true;
}

static const char *FindCmdlineValue(const char *Cmdline, const char *Name)
{
	const char *Cursor = Cmdline;
	size_t NameLength = 0;

	while (Name[NameLength] != '\0') {
		++NameLength;
	}

	while (Cursor != 0 && *Cursor != '\0') {
		const char *TokenStart;
		size_t Index = 0;

		while (*Cursor == ' ' || *Cursor == '\t') {
			++Cursor;
		}

		TokenStart = Cursor;
		while (*Cursor != '\0' && *Cursor != ' ' && *Cursor != '\t' &&
			   *Cursor != '=') {
			++Cursor;
		}

		while (Index < NameLength && TokenStart[Index] == Name[Index]) {
			++Index;
		}

		if (Index == NameLength &&
			(TokenStart[Index] == '\0' || TokenStart[Index] == ' ' ||
			 TokenStart[Index] == '\t' || TokenStart[Index] == '=')) {
			while (*Cursor == ' ' || *Cursor == '\t' || *Cursor == '=') {
				++Cursor;
			}
			return Cursor;
		}

		while (*Cursor != '\0' && *Cursor != ' ' && *Cursor != '\t') {
			++Cursor;
		}
	}

	return 0;
}

static void CopyToken(char *Destination, size_t DestinationSize,
					  const char *Source)
{
	size_t Index = 0;

	if (Destination == 0 || DestinationSize == 0) {
		return;
	}

	while (Source != 0 && Source[Index] != '\0' && Source[Index] != ' ' &&
		   Source[Index] != '\t' && Index + 1 < DestinationSize) {
		Destination[Index] = Source[Index];
		++Index;
	}

	Destination[Index] = '\0';
}

static void ResolveInitPath(const char *Cmdline, bool FramebufferPresent,
							char Destination[VfsPathMax])
{
	const char *Value = 0;

	if (FramebufferPresent) {
		Value = FindCmdlineValue(Cmdline, "fbinit");
		if (Value != 0) {
			Destination[0] = '\0';
			CopyToken(Destination, VfsPathMax, Value);
			return;
		}
	}

	Value = FindCmdlineValue(Cmdline, "init");

	Destination[0] = '\0';
	if (Value != 0) {
		CopyToken(Destination, VfsPathMax, Value);
		return;
	}

	CopyToken(Destination, VfsPathMax, BootVolumeRoot);
	CopyToken(Destination + 3, VfsPathMax - 3, "System/Bin/Shell");
}

static const BootInfo *MapBootInfo(const BootInfo *Info)
{
	uint64_t InfoAddress = BootInfoPhysical;

	if (InfoAddress < PmmGetHhdmOffset()) {
		InfoAddress += PmmGetHhdmOffset();
	}

	Info = (const BootInfo *)(uintptr_t)InfoAddress;
	if (Info == 0 || Info->Magic != BootInfoMagic) {
		FatalBoot("core.mm.paging", "failed to map boot info");
	}

	return Info;
}

static void InitStorage(void)
{
	static Fat32Volume Volumes[DiskMaxCount];
	const DiskInfo *BootDisk;
	char VolumeRoot[VfsPathMax];
	bool BootMounted = false;

	if (!DiskInit()) {
		LogWarn("core.sys.fs", "disk init failed; skipping root mount");
		return;
	}

	BootDisk = DiskGetBootDisk();
	if (BootDisk == 0 ||
		!BuildVolumeRoot(VolumeRoot, sizeof(VolumeRoot), BootDisk->VolumeId)) {
		LogWarn("core.sys.fs", "invalid boot volume");
		return;
	}

	for (size_t Index = 0; Index < DiskGetCount() && Index < DiskMaxCount;
		 ++Index) {
		const DiskInfo *Disk = DiskGetInfo(Index);

		if (Disk == 0 || Disk->Device == 0) {
			continue;
		}

		if (!Fat32Mount(&Volumes[Index], Disk->Device)) {
			LogWarn("core.sys.fs", "%s has no mountable FAT32 volume",
					Disk->Name);
			continue;
		}

		if (!VfsMount(Disk->VolumeId, Fat32GetFilesystemOps(),
					  &Volumes[Index])) {
			LogWarn("core.sys.fs", "VFS mount failed for %c:/", Disk->VolumeId);
			continue;
		}

		LogInfo("core.sys.fs", "%s mounted at %c:/", Disk->Name,
				Disk->VolumeId);
		if (Disk == BootDisk) {
			BootMounted = true;
		}
	}

	if (!BootMounted) {
		LogWarn("core.sys.fs", "boot disk has no mounted filesystem");
		return;
	}

	BuildVolumeRoot(BootVolumeRoot, sizeof(BootVolumeRoot), BootDisk->VolumeId);
	LogInfo("core.sys.fs", "boot volume mounted at %s", VolumeRoot);
}

static void InitKeyboard(void)
{
	DriverRegister(&KbdDriver);
	DeviceRegister(&KbdDevice);
	DeviceBind(&KbdDevice, &KbdDriver);

	Device *Keyboard = DeviceGet("kbd");
	if (Keyboard != 0) {
		DeviceControl(Keyboard, KbdCtrlEchoOff, 0);
		LogInfo("core.sys.entry", "keyboard input online");
	}
}

void KernelMain(const BootInfo *Info)
{
	bool WantsFramebufferConsole;

	SerialInit(SerialCom1, 115200);
	StdoutPutc = LogKernelPutc;

	if (!Info || Info->Magic != BootInfoMagic ||
		Info->Version != BootInfoVersion || Info->Size < sizeof(BootInfo)) {
		FatalBoot("core.sys.entry", "invalid boot info");
	}

	LogInit(Info->Cmdline);
	LogInfo("core.sys.entry", "kernel booting");
	LogDebug("core.sys.entry", "boot info at 0x%llx",
			 (unsigned long long)(uintptr_t)Info);
	WantsFramebufferConsole = Info->Framebuffer.Width != 0 &&
							  Info->Framebuffer.Height != 0 &&
							  Info->Framebuffer.Address != 0;
	if (!WantsFramebufferConsole) {
		TextConsoleInit();
	}
	InitFramebufferConsole(&Info->Framebuffer);
	if (Info->AcpiRsdpAddress != 0) {
		LogDebug("core.acpi", "RSDP=0x%llx",
				 (unsigned long long)Info->AcpiRsdpAddress);
	} else {
		LogDebug("core.acpi", "RSDP unavailable");
	}
	LogInfo("core.sys.entry", "Kernel cmdline=\"%s\"", Info->Cmdline);

	GdtInit();
	LogOk("core.arch.gdt", "GDT initialized");

	GdtTssInit(ReadStackPointer());
	LogTrace("core.arch.tss", "TSS initialized");

	IdtInit();
	LogOk("core.arch.idt", "IDT initialized");

	if (!PageDbInit(Info->MemoryMap, Info->MemoryMapEntriesCount,
					Info->HhdmOffset)) {
		FatalBoot("core.mm.pagedb", "failed to initialize page database");
	}
	LogOk("core.mm.pagedb", "page database online");

	if (!PmmInit(Info->HhdmOffset)) {
		FatalBoot("core.mm.pmm",
				  "failed to initialize physical memory manager");
	}
	LogOk("core.mm.pmm", "physical memory manager online");

	if (Info->AcpiRsdpAddress != 0) {
		AcpiInit(Info->AcpiRsdpAddress);
		MadtInit();
	}

	/* Enable SSE for userspace */
	uint64_t Cr4;
	__asm__ volatile("mov %%cr4, %0" : "=r"(Cr4));
	Cr4 |= (1 << 9) | (1 << 10); /* OSFXSR | OSXMMEXCPT */
	__asm__ volatile("mov %0, %%cr4" : : "r"(Cr4));

	IrqInit();
	PitInit(1000);

	BootInfoPhysical = (uint64_t)(uintptr_t)Info;

	uint64_t StackBase = PmmAllocPagesPhys(KernelStackPages);
	if (StackBase == 0) {
		FatalBoot("core.sys.entry", "failed to allocate kernel stack");
	}

	LogDebug("core.sys.entry", "switching to kernel stack 0x%llx",
			 (unsigned long long)(StackBase + (KernelStackPages * PageSize)));
	if (!PagingInit(Info, StackBase + (KernelStackPages * PageSize))) {
		FatalBoot("core.mm.paging", "failed to initialize kernel page tables");
	}

	Info = MapBootInfo(Info);

	LogOk("core.mm.paging", "kernel page tables active");

	VmmInit();
	LogOk("core.mm.vmm", "virtual memory manager online");
	if (!HeapInit()) {
		FatalBoot("core.mm.heap", "failed to initialize kernel heap");
	}
	LogOk("core.mm.heap", "kernel heap online");

	InitStorage();
	InitKeyboard();

	if (FramebufferReady()) {
		DriverRegister(&FbDriver);
		DeviceRegister(&FbDevice);
		DeviceBind(&FbDevice, &FbDriver);
		LogInfo("core.sys.entry", "framebuffer device online");
	}

	if (TextConsoleReady()) {
		DriverRegister(&TextModeConsoleDriver);
		DeviceRegister(&TextModeConsoleDevice);
		DeviceBind(&TextModeConsoleDevice, &TextModeConsoleDriver);
		LogInfo("core.sys.entry", "text console online");
	}

	FbConsoleInit(FtCtx);
	DriverRegister(&FbConsoleDriver);
	DeviceRegister(&FbConsoleDevice);
	DeviceBind(&FbConsoleDevice, &FbConsoleDriver);
	LogInfo("core.sys.entry", "framebuffer console online as fbconsole");

	DriverRegister(&MouseDriver);
	DeviceRegister(&MouseDevice);
	DeviceBind(&MouseDevice, &MouseDriver);
	LogInfo("core.sys.entry", "mouse device online");

	if (!SchedulerInit()) {
		FatalBoot("core.sched", "failed to initialize scheduler");
	}

	char InitPath[VfsPathMax];
	ResolveInitPath(Info->Cmdline, FramebufferReady(), InitPath);
	if (ElfSpawn(InitPath) == 0) {
		LogFatal("core.sys.entry", "failed to launch init '%s'", InitPath);
		HaltForever();
	}
	LogInfo("core.sys.entry", "init process '%s' launched", InitPath);

	LogInfo("core.sys.entry", "avOS kernel v1.0 finished initializing!");

	__asm__ volatile("sti");
	SchedulerStart();
	HaltForever();
}
