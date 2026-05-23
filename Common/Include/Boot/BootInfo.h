#ifndef BOOT_BOOTINFO_H
#define BOOT_BOOTINFO_H

#include <stdint.h>

#define BootInfoMagic 0x41564f53u
#define BootInfoVersion 3u
#define BootInfoKernelPathSize 128u
#define BootMemoryMapMaxEntries 32u
#define BootFramebufferWidth 1024
#define BootFramebufferHeight 768
#define BootFramebufferBpp 32
#define BootFramebufferPitch (BootFramebufferWidth * (BootFramebufferBpp / 8))

typedef enum BootMemoryType {
	BootMemoryTypeUsable = 1,
	BootMemoryTypeReserved = 2,
	BootMemoryTypeAcpiReclaimable = 3,
	BootMemoryTypeAcpiNvs = 4,
	BootMemoryTypeBad = 5,
} BootMemoryType;

typedef struct BootMemoryMapEntry {
	uint64_t Base;
	uint64_t Length;
	uint32_t Type;
} __attribute__((packed)) BootMemoryMapEntry;

typedef struct BootFramebuffer {
	uint64_t Address;
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	uint8_t Bpp;
} BootFramebuffer;

typedef struct BootInfo {
	uint32_t Magic;
	uint32_t Version;
	uint32_t Size;
	uint32_t MemoryMapEntriesCount;
	uint64_t KernelEntry;
	char KernelPath[BootInfoKernelPathSize];
	BootMemoryMapEntry MemoryMap[BootMemoryMapMaxEntries];
	BootFramebuffer Framebuffer;
} BootInfo;

#endif
