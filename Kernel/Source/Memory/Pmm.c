#include <Memory/Pmm.h>

#include <Boot/BootInfo.h>
#include <Core/Log.h>

#include <stddef.h>
#include <stdint.h>

static Page *FreeList;
static uint64_t FreePages;
static uint64_t HhdmOffset;

static void RebuildFreeList(void)
{
	FreeList = 0;
	FreePages = 0;

	for (uint64_t Pfn = 0; Pfn < PageDbGetPageCount(); ++Pfn) {
		Page *PageEntry = PageDbPfnToPage(Pfn);
		if (!PageDbPageHasState(PageEntry, PageFree)) {
			if (PageEntry != 0) {
				PageEntry->FreeNext = 0;
			}
			continue;
		}

		PageEntry->FreeNext = FreeList;
		FreeList = PageEntry;
		++FreePages;
	}
}

static Page *FindContiguousFreePages(uint64_t PagesCount)
{
	uint64_t RunStartPfn = 0;
	uint64_t RunLength = 0;

	if (PagesCount == 0 || PagesCount > FreePages) {
		return 0;
	}

	for (uint64_t Pfn = 0; Pfn < PageDbGetPageCount(); ++Pfn) {
		Page *PageEntry = PageDbPfnToPage(Pfn);
		if (PageDbPageHasState(PageEntry, PageFree)) {
			if (RunLength == 0) {
				RunStartPfn = Pfn;
			}

			++RunLength;
			if (RunLength == PagesCount) {
				return PageDbPfnToPage(RunStartPfn);
			}
			continue;
		}

		RunLength = 0;
	}

	return 0;
}

static bool TrySetContiguousState(Page *FirstPage, uint64_t PagesCount,
								  uint64_t OldState, uint64_t NewState)
{
	uint64_t FirstPfn = PageDbPageToPfn(FirstPage);

	if (FirstPfn == UINT64_MAX || PagesCount == 0 ||
		PagesCount > PageDbGetPageCount() - FirstPfn) {
		return false;
	}

	for (uint64_t Index = 0; Index < PagesCount; ++Index) {
		Page *PageEntry = PageDbPfnToPage(FirstPfn + Index);
		if (!PageDbPageHasState(PageEntry, OldState)) {
			return false;
		}
	}

	for (uint64_t Index = 0; Index < PagesCount; ++Index) {
		PageDbTrySetPageState(PageDbPfnToPage(FirstPfn + Index), OldState,
							  NewState);
	}

	return true;
}

bool PmmInit(uint64_t NewHhdmOffset)
{
	if (PageDbGet() == 0) {
		LogError("core.mm.pmm", "init rejected: page database unavailable");
		return false;
	}

	if (NewHhdmOffset == 0) {
		LogError("core.mm.pmm", "init rejected: missing HHDM offset");
		return false;
	}

	HhdmOffset = NewHhdmOffset;
	RebuildFreeList();

	LogDebug("core.mm.pmm", "free pages=%llu (%llu MiB) hhdm=0x%llx",
			 (unsigned long long)FreePages,
			 (unsigned long long)((FreePages * PageSize) >> 20),
			 (unsigned long long)HhdmOffset);
	return true;
}

Page *PmmAllocPage(void)
{
	while (FreeList != 0) {
		Page *PageEntry = FreeList;
		FreeList = PageEntry->FreeNext;
		PageEntry->FreeNext = 0;

		if (!PageDbTrySetPageState(PageEntry, PageFree, PageUsed)) {
			LogWarn("core.mm.pmm", "skipping corrupt free-list page pfn=0x%llx",
					(unsigned long long)PageDbPageToPfn(PageEntry));
			continue;
		}

		--FreePages;
		return PageEntry;
	}

	LogWarn("core.mm.pmm", "out of physical pages");
	return 0;
}

Page *PmmAllocPages(uint64_t PagesCount)
{
	if (PagesCount == 1) {
		return PmmAllocPage();
	}

	Page *FirstPage = FindContiguousFreePages(PagesCount);
	if (FirstPage == 0) {
		LogWarn("core.mm.pmm", "out of contiguous physical pages count=%llu",
				(unsigned long long)PagesCount);
		return 0;
	}

	if (!TrySetContiguousState(FirstPage, PagesCount, PageFree, PageUsed)) {
		LogWarn("core.mm.pmm", "contiguous run changed while allocating count=%llu",
				(unsigned long long)PagesCount);
		return 0;
	}

	RebuildFreeList();
	return FirstPage;
}

uint64_t PmmAllocPagePhys(void)
{
	uint64_t PhysicalAddress = PmmAllocPageRawPhys();
	if (PhysicalAddress == 0) {
		return 0;
	}

	return PmmPhysToHhdm(PhysicalAddress);
}

uint64_t PmmAllocPagesPhys(uint64_t PagesCount)
{
	uint64_t PhysicalAddress = PmmAllocPagesRawPhys(PagesCount);
	if (PhysicalAddress == 0) {
		return 0;
	}

	return PmmPhysToHhdm(PhysicalAddress);
}

uint64_t PmmAllocPageRawPhys(void)
{
	Page *PageEntry = PmmAllocPage();
	if (PageEntry == 0) {
		return 0;
	}

	return PageDbPageToPhys(PageEntry);
}

uint64_t PmmAllocPagesRawPhys(uint64_t PagesCount)
{
	Page *PageEntry = PmmAllocPages(PagesCount);
	if (PageEntry == 0) {
		return 0;
	}

	return PageDbPageToPhys(PageEntry);
}

bool PmmFreePage(Page *PageEntry)
{
	if (!PageDbTrySetPageState(PageEntry, PageUsed, PageFree)) {
		LogWarn("core.mm.pmm", "refusing invalid free pfn=0x%llx",
				(unsigned long long)PageDbPageToPfn(PageEntry));
		return false;
	}

	PageEntry->FreeNext = FreeList;
	FreeList = PageEntry;
	++FreePages;
	return true;
}

bool PmmFreePages(Page *PageEntry, uint64_t PagesCount)
{
	if (PagesCount == 1) {
		return PmmFreePage(PageEntry);
	}

	if (!TrySetContiguousState(PageEntry, PagesCount, PageUsed, PageFree)) {
		LogWarn("core.mm.pmm", "refusing invalid contiguous free pfn=0x%llx count=%llu",
				(unsigned long long)PageDbPageToPfn(PageEntry),
				(unsigned long long)PagesCount);
		return false;
	}

	RebuildFreeList();
	return true;
}

bool PmmFreePagePhys(uint64_t PhysicalAddress)
{
	PhysicalAddress = PmmHhdmToPhys(PhysicalAddress);

	if ((PhysicalAddress & (PageSize - 1ull)) != 0) {
		LogWarn("core.mm.pmm", "refusing unaligned free phys=0x%llx",
				(unsigned long long)PhysicalAddress);
		return false;
	}

	return PmmFreePage(PageDbPhysToPage(PhysicalAddress));
}

bool PmmFreePagesPhys(uint64_t PhysicalAddress, uint64_t PagesCount)
{
	PhysicalAddress = PmmHhdmToPhys(PhysicalAddress);

	if ((PhysicalAddress & (PageSize - 1ull)) != 0) {
		LogWarn("core.mm.pmm", "refusing unaligned free phys=0x%llx",
				(unsigned long long)PhysicalAddress);
		return false;
	}

	return PmmFreePages(PageDbPhysToPage(PhysicalAddress), PagesCount);
}

uint64_t PmmPhysToHhdm(uint64_t PhysicalAddress)
{
	if (PhysicalAddress == 0 || HhdmOffset == 0 ||
		PhysicalAddress >= BootHhdmMappedSize) {
		return 0;
	}

	return PhysicalAddress + HhdmOffset;
}

uint64_t PmmHhdmToPhys(uint64_t Address)
{
	if (HhdmOffset != 0 && Address >= HhdmOffset &&
		Address < HhdmOffset + BootHhdmMappedSize) {
		return Address - HhdmOffset;
	}

	return Address;
}

uint64_t PmmGetHhdmOffset(void)
{
	return HhdmOffset;
}

uint64_t PmmGetFreePageCount(void)
{
	return FreePages;
}
