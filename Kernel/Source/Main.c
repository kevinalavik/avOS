#include <Boot/BootInfo.h>
#include <Device/Serial.h>
#include <Library/Printf.h>
#include <Library/Stdout.h>
#include <Core/Log.h>
#include <Arch/Gdt.h>
#include <Arch/Idt.h>
#include <Device/Framebuffer.h>
#include <Device/TextConsole.h>
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

static void HaltFatal(const char *Component, const char *Message)
{
	LogFatal(Component, "%s", Message);
	for (;;)
		__asm__ volatile("hlt");
}

static void InitFramebufferConsole(const BootFramebuffer *Framebuffer)
{
	FtCtx = NULL;

	if (FramebufferInit(Framebuffer)) {
		LogInfo(
			"FB", "framebuffer: %ux%u pitch=%u bpp=%u addr=0x%llx",
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
			LogInfo("FB", "flanterm terminal initialized");
		} else {
			LogWarn("FB", "flanterm init returned null");
		}
	} else {
		LogWarn("FB", "framebuffer unavailable");
	}
}

void KernelMain(const BootInfo *Info)
{
	LogInit();
	LogSetLevel(LogLevelTrace);

	SerialInit(SerialCom1, 115200);
	TextConsoleInit();
	StdoutPutc = LogKernelPutc;

	if (!Info || Info->Magic != BootInfoMagic ||
		Info->Version != BootInfoVersion || Info->Size < sizeof(BootInfo)) {
		LogFatal("ENTRY", "invalid boot info");
		for (;;)
			__asm__ volatile("hlt");
	}

	LogDebug("ENTRY", "hello from kernel");
	InitFramebufferConsole(&Info->Framebuffer);

	GdtInit();
	LogOk("ARCH", "GDT initialized");

	GdtTssInit(ReadStackPointer());
	LogOk("ARCH", "TSS initialized");

	IdtInit();
	LogOk("ARCH", "IDT initialized");

	if (!PageDbInit(Info->MemoryMap, Info->MemoryMapEntriesCount,
					Info->HhdmOffset)) {
		LogFatal("PAGEDB", "failed to initialize page database");
		for (;;)
			__asm__ volatile("hlt");
	}

	if (!PmmInit(Info->HhdmOffset)) {
		LogFatal("PMM", "failed to initialize physical memory manager");
		for (;;)
			__asm__ volatile("hlt");
	}

	BootInfoPhysical = (uint64_t)(uintptr_t)Info;

	uint64_t StackBase = PmmAllocPagesPhys(KernelStackPages);
	if (StackBase == 0) {
		LogFatal("ENTRY", "failed to allocate kernel stack");
		for (;;)
			__asm__ volatile("hlt");
	}

	LogInfo("ENTRY", "switching to kernel stack 0x%llx",
			(unsigned long long)(StackBase + (KernelStackPages * PageSize)));
	if (!PagingInit(Info, StackBase + (KernelStackPages * PageSize))) {
		LogFatal("PAGING", "failed to initialize kernel page tables");
		for (;;)
			__asm__ volatile("hlt");
	}

	uint64_t InfoAddress = BootInfoPhysical;
	if (InfoAddress < PmmGetHhdmOffset()) {
		InfoAddress += PmmGetHhdmOffset();
	}
	Info = (const BootInfo *)(uintptr_t)InfoAddress;
	if (Info == 0 || Info->Magic != BootInfoMagic) {
		LogFatal("PAGING", "failed to map boot info");
		for (;;)
			__asm__ volatile("hlt");
	}

	LogOk("PAGING", "kernel page tables active");

	VmmInit();
	if (!HeapInit()) {
		LogFatal("HEAP", "failed to initialize kernel heap");
		for (;;)
			__asm__ volatile("hlt");
	}
	LogOk("HEAP", "kernel heap online");

	uint64_t *a = KernelAlloc(1);
	LogInfo("TEST", "Allocated 1 byte at %p using kernel heap", a);
	*a = 42;
	LogInfo("TEST", "Wrote %d to %p", *a, a);
	KernelFree(a);
	LogInfo("TEST", "Freed using heap!");

	for (;;) {
		__asm__ volatile("hlt");
	}
}
