#ifndef MEMORY_PAGEDB_H
#define MEMORY_PAGEDB_H

#include <Boot/BootInfo.h>

#include <stdbool.h>
#include <stdint.h>

#define PageSize 0x1000ull
#define PageShift 12u

#define PageFree (1ull << 0)
#define PageUsed (1ull << 1)
#define PageReserved (1ull << 2)

#define PageStateMask (PageFree | PageUsed | PageReserved)

typedef struct Page {
	struct Page *Next;
	struct Page *FreeNext;
	uint64_t Flags;
} Page;

bool PageDbInit(const BootMemoryMapEntry *MemoryMap, uint32_t EntriesCount,
				uint64_t HhdmOffset);
void PageDbMarkRange(uint64_t Base, uint64_t Length, uint64_t Flags);
bool PageDbPageIsValid(const Page *PageEntry);
bool PageDbPageHasState(const Page *PageEntry, uint64_t State);
bool PageDbTrySetPageState(Page *PageEntry, uint64_t OldState,
						   uint64_t NewState);

Page *PageDbGet(void);
Page *PageDbGetFirst(void);
Page *PageDbPfnToPage(uint64_t Pfn);
Page *PageDbPhysToPage(uint64_t PhysicalAddress);

uint64_t PageDbGetMaxPfn(void);
uint64_t PageDbGetPageCount(void);
uint64_t PageDbPageToPfn(const Page *PageEntry);
uint64_t PageDbPageToPhys(const Page *PageEntry);
uint64_t PageDbPfnToPhys(uint64_t Pfn);
uint64_t PageDbGetPageFlags(const Page *PageEntry);
uint64_t PageDbGetStoragePhys(void);
uint64_t PageDbGetStorageBytes(void);
uint64_t PageDbGetUsablePages(void);

#endif
