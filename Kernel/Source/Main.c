#include <Boot/BootInfo.h>
#include <Device/Serial.h>
#include <Library/Printf.h>
#include <Library/Stdout.h>
#include <Core/Log.h>
#include <Arch/Gdt.h>
#include <Arch/Idt.h>
#include <Device/Framebuffer.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stddef.h>

static struct flanterm_context *FtCtx;

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
	}
}

static uint64_t ReadStackPointer(void)
{
	uint64_t StackPointer;

	__asm__ volatile("movq %%rsp, %0" : "=r"(StackPointer));
	return StackPointer;
}

void KernelMain(const BootInfo *Info)
{
	LogInit();
	LogSetLevel(LogLevelTrace);

	SerialInit(SerialCom1, 115200);
	StdoutPutc = SerialPutc;

	LogDebug("hello from kernel!");

	if (!Info || Info->Magic != BootInfoMagic ||
		Info->Version != BootInfoVersion || Info->Size < sizeof(BootInfo)) {
		LogFatal("invalid boot info");
		for (;;)
			__asm__ volatile("hlt");
	}

	if (FramebufferInit(&Info->Framebuffer)) {
		LogInfo("framebuffer: %ux%u pitch=%u bpp=%u addr=0x%llx",
				(unsigned int)Info->Framebuffer.Width,
				(unsigned int)Info->Framebuffer.Height,
				(unsigned int)Info->Framebuffer.Pitch,
				(unsigned int)Info->Framebuffer.Bpp,
				(unsigned long long)Info->Framebuffer.Address);

		FtCtx = flanterm_fb_init(
			NULL, NULL, (uint32_t *)(uintptr_t)Info->Framebuffer.Address,
			Info->Framebuffer.Width, Info->Framebuffer.Height,
			Info->Framebuffer.Pitch, 8, 16, 8, 8, 8, 0, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0, 0);

		if (FtCtx != NULL) {
			StdoutPutc = LogKernelPutc;
			LogInfo("flanterm terminal initialized");
		} else {
			LogWarn("flanterm init returned null");
		}
	} else {
		LogWarn("framebuffer unavailable");
	}

	uint64_t UsableBytes = 0;
	for (uint32_t Index = 0;
		 Index < Info->MemoryMapEntriesCount && Index < BootMemoryMapMaxEntries;
		 ++Index) {
		const BootMemoryMapEntry *Entry = &Info->MemoryMap[Index];

		if (Entry->Type == BootMemoryTypeUsable) {
			UsableBytes += Entry->Length;
		}
	}

	LogInfo("usable memory: %llu MiB", (unsigned long long)(UsableBytes >> 20));

	GdtInit();
	LogOk("GDT initialized");

	GdtTssInit(ReadStackPointer());
	LogOk("TSS initialized");

	IdtInit();
	LogOk("IDT initialized");

	*(uint64_t *)0xdeadbeef = 42;

	for (;;) {
		__asm__ volatile("hlt");
	}
}
