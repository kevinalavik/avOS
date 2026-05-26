#ifndef MEMORY_PAGING_H
#define MEMORY_PAGING_H

#include <Boot/BootInfo.h>

#include <stdbool.h>
#include <stdint.h>

#define PagingFlagPresent 0x001ull
#define PagingFlagWritable 0x002ull
#define PagingFlagUser 0x004ull
#define PagingFlagWriteThrough 0x008ull
#define PagingFlagCacheDisable 0x010ull
#define PagingFlagGlobal 0x100ull
#define PagingFlagNoExecute (1ull << 63)

bool PagingInit(const BootInfo *Info, uint64_t StackTop);
uint64_t PagingGetRootTablePhys(void);

bool PagingMapPage(uint64_t VirtualAddress, uint64_t PhysicalAddress,
				   uint64_t Flags);
bool PagingMapRange(uint64_t VirtualBase, uint64_t PhysicalBase, uint64_t Size,
					uint64_t Flags);
void PagingUnmapPage(uint64_t VirtualAddress);
void PagingUnmapRange(uint64_t VirtualBase, uint64_t Size);
uint64_t PagingVirtToPhys(uint64_t VirtualAddress);
uint64_t PagingGetFlags(uint64_t VirtualAddress);

#endif
