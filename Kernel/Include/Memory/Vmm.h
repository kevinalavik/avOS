#ifndef MEMORY_VMM_H
#define MEMORY_VMM_H

#include <stdbool.h>
#include <stdint.h>

#define VmmKernelArenaBase 0xffff900000000000ull
#define VmmKernelArenaSize 0x0000001000000000ull

typedef enum VmmRegionFlags {
	VmmRegionFree = 0,
	VmmRegionReserved = 1ull << 0,
	VmmRegionMapped = 1ull << 1,
	VmmRegionWritable = 1ull << 2,
	VmmRegionKernelHeap = 1ull << 3,
	VmmRegionAnonymous = 1ull << 4,
	VmmRegionDevice = 1ull << 5,
} VmmRegionFlags;

typedef struct VmmRegion {
	uint64_t Base;
	uint64_t Size;
	uint64_t Committed;
	uint64_t Flags;
} VmmRegion;

void VmmInit(void);
uint64_t VmmReserveRegion(uint64_t Size, uint64_t Flags);
bool VmmCommitRange(uint64_t Base, uint64_t Size, uint64_t PagingFlags);
bool VmmUncommitRange(uint64_t Base, uint64_t Size);
uint64_t VmmMapAnonymous(uint64_t Size, uint64_t RegionFlags,
						 uint64_t PagingFlags);
uint64_t VmmMapPhysicalRange(uint64_t PhysicalBase, uint64_t Size,
							 uint64_t RegionFlags, uint64_t PagingFlags);
bool VmmUnmapRegion(uint64_t Base);
bool VmmFreeRegion(uint64_t Base);

const VmmRegion *VmmFindRegion(uint64_t Address);
bool VmmRangeMapped(uint64_t Base, uint64_t Size);
uint64_t VmmGetRegionCount(void);

#endif
