#ifndef MEMORY_PMM_H
#define MEMORY_PMM_H

#include <Memory/PageDb.h>

#include <stdbool.h>
#include <stdint.h>

bool PmmInit(uint64_t HhdmOffset);

Page *PmmAllocPage(void);
Page *PmmAllocPages(uint64_t PagesCount);
uint64_t PmmAllocPagePhys(void);
uint64_t PmmAllocPagesPhys(uint64_t PagesCount);
uint64_t PmmAllocPageRawPhys(void);
uint64_t PmmAllocPagesRawPhys(uint64_t PagesCount);

bool PmmFreePage(Page *PageEntry);
bool PmmFreePages(Page *PageEntry, uint64_t PagesCount);
bool PmmFreePagePhys(uint64_t PhysicalAddress);
bool PmmFreePagesPhys(uint64_t PhysicalAddress, uint64_t PagesCount);

uint64_t PmmPhysToHhdm(uint64_t PhysicalAddress);
uint64_t PmmHhdmToPhys(uint64_t Address);
uint64_t PmmGetHhdmOffset(void);
uint64_t PmmGetFreePageCount(void);

#endif
