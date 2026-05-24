#include <Acpi/Acpi.h>
#include <Arch/Gdt.h>
#include <Arch/Idt.h>
#include <Boot/BootInfo.h>
#include <Core/Log.h>
#include <Device/Framebuffer.h>
#include <Device/Serial.h>
#include <Device/TextConsole.h>
#include <Library/Printf.h>
#include <Library/Stdout.h>
#include <Memory/Heap.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Pmm.h>
#include <Memory/Vmm.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <stdbool.h>
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

void KernelMain(const BootInfo *Info)
{
	SerialInit(SerialCom1, 115200);
	TextConsoleInit();
	StdoutPutc = LogKernelPutc;

	if (!Info || Info->Magic != BootInfoMagic ||
		Info->Version != BootInfoVersion || Info->Size < sizeof(BootInfo)) {
		LogFatal("core.sys.entry", "invalid boot info");
		for (;;)
			__asm__ volatile("hlt");
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

	GdtInit();
	LogOk("core.arch.gdt", "GDT initialized");

	GdtTssInit(ReadStackPointer());
	LogTrace("core.arch.tss", "TSS initialized");

	IdtInit();
	LogOk("core.arch.idt", "IDT initialized");

	if (!PageDbInit(Info->MemoryMap, Info->MemoryMapEntriesCount,
					Info->HhdmOffset)) {
		LogFatal("core.mm.pagedb", "failed to initialize page database");
		for (;;)
			__asm__ volatile("hlt");
	}
	LogOk("core.mm.pagedb", "page database online");

	if (!PmmInit(Info->HhdmOffset)) {
		LogFatal("core.mm.pmm", "failed to initialize physical memory manager");
		for (;;)
			__asm__ volatile("hlt");
	}
	LogOk("core.mm.pmm", "physical memory manager online");

	BootInfoPhysical = (uint64_t)(uintptr_t)Info;

	uint64_t StackBase = PmmAllocPagesPhys(KernelStackPages);
	if (StackBase == 0) {
		LogFatal("core.sys.entry", "failed to allocate kernel stack");
		for (;;)
			__asm__ volatile("hlt");
	}

	LogDebug("core.sys.entry", "switching to kernel stack 0x%llx",
			 (unsigned long long)(StackBase + (KernelStackPages * PageSize)));
	if (!PagingInit(Info, StackBase + (KernelStackPages * PageSize))) {
		LogFatal("core.mm.paging", "failed to initialize kernel page tables");
		for (;;)
			__asm__ volatile("hlt");
	}

	uint64_t InfoAddress = BootInfoPhysical;
	if (InfoAddress < PmmGetHhdmOffset()) {
		InfoAddress += PmmGetHhdmOffset();
	}
	Info = (const BootInfo *)(uintptr_t)InfoAddress;
	if (Info == 0 || Info->Magic != BootInfoMagic) {
		LogFatal("core.mm.paging", "failed to map boot info");
		for (;;)
			__asm__ volatile("hlt");
	}

	LogOk("core.mm.paging", "kernel page tables active");

	VmmInit();
	LogOk("core.mm.vmm", "virtual memory manager online");
	if (!HeapInit()) {
		LogFatal("core.mm.heap", "failed to initialize kernel heap");
		for (;;)
			__asm__ volatile("hlt");
	}
	LogOk("core.mm.heap", "kernel heap online");

	if (Info->AcpiRsdpAddress != 0) {
		AcpiInit(Info->AcpiRsdpAddress);
	}

	for (;;) {
		__asm__ volatile("hlt");
	}
}
