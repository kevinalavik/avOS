#include <Acpi/Acpi.h>
#include <Acpi/Madt.h>
#include <Arch/Gdt.h>
#include <Arch/Idt.h>
#include <Arch/Irq.h>
#include <Boot/BootInfo.h>
#include <Core/Ksh.h>
#include <Core/Log.h>
#include <Device/Framebuffer.h>
#include <Device/Pit.h>
#include <Device/Serial.h>
#include <Device/TextConsole.h>
#include <Drivers/Device.h>
#include <Drivers/Input/Keyboard.h>
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
	if (FtCtx != NULL) {
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
			LogWarn("core.sys.fs", "VFS mount failed for %c:/",
					Disk->VolumeId);
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

	if (!KshInit(BootDisk->VolumeId)) {
		LogWarn("core.sys.fs", "shell path initialization failed");
		return;
	}

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
	SerialInit(SerialCom1, 115200);
	TextConsoleInit();
	StdoutPutc = LogKernelPutc;

	if (!Info || Info->Magic != BootInfoMagic ||
		Info->Version != BootInfoVersion || Info->Size < sizeof(BootInfo)) {
		FatalBoot("core.sys.entry", "invalid boot info");
	}

	LogInit(Info->Cmdline);
	LogInfo("core.sys.entry", "kernel booting");
	LogDebug("core.sys.entry", "boot info at 0x%llx",
			 (unsigned long long)(uintptr_t)Info);
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

	IrqInit();
	/* PIT is used for short I/O delays (does not require IRQs enabled). */
	PitInit(100);

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

	LogInfo("core.sys.entry", "avOS kernel v1.0 finished initializing!");

	__asm__ volatile("sti");
	KshRun();
	HaltForever();
}
