#include <Memory/PageDb.h>

#include <Core/Log.h>

#include <stddef.h>
#include <stdint.h>

#define PageDbLowMappedLimit 0x40000000ull

static Page *Pages;
static uint64_t MaxPfn;
static uint64_t PageCount;
static uint64_t StoragePhys;
static uint64_t StorageBytes;
static uint64_t UsablePages;
static uint64_t HhdmBase;

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static bool RangeEnd(uint64_t Base, uint64_t Length, uint64_t *EndOut)
{
	if (Length == 0 || Base > UINT64_MAX - Length) {
		return false;
	}

	*EndOut = Base + Length;
	return true;
}

static const char *MemoryTypeName(uint32_t Type)
{
	switch (Type) {
	case BootMemoryTypeUsable:
		return "usable";
	case BootMemoryTypeReserved:
		return "reserved";
	case BootMemoryTypeAcpiReclaimable:
		return "acpi";
	case BootMemoryTypeAcpiNvs:
		return "acpi-nvs";
	case BootMemoryTypeBad:
		return "bad";
	default:
		return "unknown";
	}
}

static uint64_t PageFlagsForMemoryType(uint32_t Type)
{
	if (Type == BootMemoryTypeUsable) {
		return PageFree;
	}

	return PageReserved;
}

static bool FindMemoryMapLimit(const BootMemoryMapEntry *MemoryMap,
							   uint32_t EntriesCount)
{
	uint64_t HighestEnd = 0;

	for (uint32_t Index = 0;
		 Index < EntriesCount && Index < BootMemoryMapMaxEntries; ++Index) {
		const BootMemoryMapEntry *Entry = &MemoryMap[Index];
		uint64_t End;

		if (!RangeEnd(Entry->Base, Entry->Length, &End)) {
			LogWarn("core.mm.pagedb", "dropping invalid memory range %u",
					(unsigned int)Index);
			continue;
		}

		LogDebug("core.mm.pagedb", "memmap[%u]: %s 0x%llx-0x%llx", (unsigned int)Index,
				 MemoryTypeName(Entry->Type), (unsigned long long)Entry->Base,
				 (unsigned long long)End);

		if (Entry->Type == BootMemoryTypeUsable && End > HighestEnd) {
			HighestEnd = End;
		}
	}

	if (HighestEnd == 0) {
		return false;
	}

	MaxPfn = (AlignUp(HighestEnd, PageSize) >> PageShift) - 1ull;
	PageCount = MaxPfn + 1ull;
	return true;
}

static bool ReserveStorage(const BootMemoryMapEntry *MemoryMap,
						   uint32_t EntriesCount)
{
	StorageBytes = AlignUp(PageCount * sizeof(Page), PageSize);

	for (uint32_t Index = 0;
		 Index < EntriesCount && Index < BootMemoryMapMaxEntries; ++Index) {
		const BootMemoryMapEntry *Entry = &MemoryMap[Index];
		uint64_t End;

		if (Entry->Type != BootMemoryTypeUsable ||
			!RangeEnd(Entry->Base, Entry->Length, &End)) {
			continue;
		}

		uint64_t Base = AlignUp(Entry->Base, PageSize);
		if (End > PageDbLowMappedLimit) {
			End = PageDbLowMappedLimit;
		}

		if (Base < End && StorageBytes <= End - Base) {
			StoragePhys = Base;
			Pages = (Page *)(uintptr_t)(StoragePhys + HhdmBase);
			return true;
		}
	}

	return false;
}

void PageDbMarkRange(uint64_t Base, uint64_t Length, uint64_t Flags)
{
	uint64_t End;

	if (Pages == 0 || !RangeEnd(Base, Length, &End)) {
		return;
	}

	uint64_t StartPfn = AlignDown(Base, PageSize) >> PageShift;
	uint64_t EndPfn = AlignUp(End, PageSize) >> PageShift;

	if (StartPfn > MaxPfn) {
		return;
	}
	if (EndPfn > PageCount) {
		EndPfn = PageCount;
	}

	for (uint64_t Pfn = StartPfn; Pfn < EndPfn; ++Pfn) {
		Pages[Pfn].Flags = (Pages[Pfn].Flags & ~PageStateMask) | Flags;
	}
}

bool PageDbPageIsValid(const Page *PageEntry)
{
	return Pages != 0 && PageEntry >= Pages && PageEntry <= &Pages[MaxPfn];
}

uint64_t PageDbGetPageFlags(const Page *PageEntry)
{
	if (!PageDbPageIsValid(PageEntry)) {
		return 0;
	}

	return PageEntry->Flags;
}

bool PageDbPageHasState(const Page *PageEntry, uint64_t State)
{
	if (!PageDbPageIsValid(PageEntry)) {
		return false;
	}

	return (PageEntry->Flags & PageStateMask) == State;
}

bool PageDbTrySetPageState(Page *PageEntry, uint64_t OldState,
						   uint64_t NewState)
{
	if (!PageDbPageIsValid(PageEntry)) {
		return false;
	}

	if ((PageEntry->Flags & PageStateMask) != OldState) {
		return false;
	}

	PageEntry->Flags = (PageEntry->Flags & ~PageStateMask) | NewState;
	return true;
}

bool PageDbInit(const BootMemoryMapEntry *MemoryMap, uint32_t EntriesCount,
				uint64_t HhdmOffset)
{
	if (MemoryMap == 0 || EntriesCount == 0) {
		LogError("core.mm.pagedb", "init rejected: missing memory map");
		return false;
	}

	if (HhdmOffset == 0) {
		LogError("core.mm.pagedb", "init rejected: missing HHDM offset");
		return false;
	}

	HhdmBase = HhdmOffset;

	if (!FindMemoryMapLimit(MemoryMap, EntriesCount) ||
		!ReserveStorage(MemoryMap, EntriesCount)) {
		LogError("core.mm.pagedb", "no usable low-memory range for page database");
		return false;
	}

	for (uint64_t Pfn = 0; Pfn < PageCount; ++Pfn) {
		Pages[Pfn].Next = (Pfn + 1ull < PageCount) ? &Pages[Pfn + 1ull] : 0;
		Pages[Pfn].FreeNext = 0;
		Pages[Pfn].Flags = PageReserved;
	}

	UsablePages = 0;
	for (uint32_t Index = 0;
		 Index < EntriesCount && Index < BootMemoryMapMaxEntries; ++Index) {
		const BootMemoryMapEntry *Entry = &MemoryMap[Index];
		uint64_t End;

		if (!RangeEnd(Entry->Base, Entry->Length, &End)) {
			continue;
		}

		PageDbMarkRange(Entry->Base, Entry->Length,
						PageFlagsForMemoryType(Entry->Type));
		if (Entry->Type == BootMemoryTypeUsable) {
			UsablePages +=
				(AlignUp(End, PageSize) - AlignDown(Entry->Base, PageSize)) >>
				PageShift;
		}
	}

	PageDbMarkRange(StoragePhys, StorageBytes, PageUsed);

	LogDebug("core.mm.pagedb", "pages=%llu max_pfn=0x%llx db=0x%llx bytes=%llu",
			 (unsigned long long)PageCount, (unsigned long long)MaxPfn,
			 (unsigned long long)StoragePhys, (unsigned long long)StorageBytes);
	LogDebug("core.mm.pagedb", "usable pages=%llu entry=%u bytes",
			 (unsigned long long)UsablePages, (unsigned int)sizeof(Page));

	return true;
}

Page *PageDbGet(void)
{
	return Pages;
}

Page *PageDbGetFirst(void)
{
	return Pages;
}

Page *PageDbPfnToPage(uint64_t Pfn)
{
	if (Pages == 0 || Pfn > MaxPfn) {
		return 0;
	}

	return &Pages[Pfn];
}

Page *PageDbPhysToPage(uint64_t PhysicalAddress)
{
	return PageDbPfnToPage(PhysicalAddress >> PageShift);
}

uint64_t PageDbGetMaxPfn(void)
{
	return MaxPfn;
}

uint64_t PageDbGetPageCount(void)
{
	return PageCount;
}

uint64_t PageDbPageToPfn(const Page *PageEntry)
{
	if (!PageDbPageIsValid(PageEntry)) {
		return UINT64_MAX;
	}

	return (uint64_t)(PageEntry - Pages);
}

uint64_t PageDbPageToPhys(const Page *PageEntry)
{
	uint64_t Pfn = PageDbPageToPfn(PageEntry);
	if (Pfn == UINT64_MAX) {
		return 0;
	}

	return PageDbPfnToPhys(Pfn);
}

uint64_t PageDbPfnToPhys(uint64_t Pfn)
{
	if (Pfn > MaxPfn) {
		return 0;
	}

	return Pfn << PageShift;
}

uint64_t PageDbGetStoragePhys(void)
{
	return StoragePhys;
}

uint64_t PageDbGetStorageBytes(void)
{
	return StorageBytes;
}

uint64_t PageDbGetUsablePages(void)
{
	return UsablePages;
}
