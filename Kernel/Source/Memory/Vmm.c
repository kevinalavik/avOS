#include <Memory/Vmm.h>

#include <Core/Log.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Pmm.h>

#include <stddef.h>
#include <stdint.h>

#define VmmMaxRegions 256u

static VmmRegion Regions[VmmMaxRegions];
static uint64_t RegionCount;

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static bool CheckedEnd(uint64_t Base, uint64_t Size, uint64_t *EndOut)
{
	if (Size == 0 || Base > UINT64_MAX - Size) {
		return false;
	}

	*EndOut = Base + Size;
	return true;
}

static bool RangesOverlap(uint64_t BaseA, uint64_t SizeA, uint64_t BaseB,
						  uint64_t SizeB)
{
	uint64_t EndA;
	uint64_t EndB;

	if (!CheckedEnd(BaseA, SizeA, &EndA) || !CheckedEnd(BaseB, SizeB, &EndB)) {
		return true;
	}

	return BaseA < EndB && BaseB < EndA;
}

static VmmRegion *AllocRegionSlot(void)
{
	for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
		if ((Regions[Index].Flags & VmmRegionReserved) == 0) {
			return &Regions[Index];
		}
	}

	return 0;
}

static VmmRegion *FindRegionByBase(uint64_t Base)
{
	for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
		if ((Regions[Index].Flags & VmmRegionReserved) != 0 &&
			Regions[Index].Base == Base) {
			return &Regions[Index];
		}
	}

	return 0;
}

static VmmRegion *FindRegionForRange(uint64_t Base, uint64_t Size)
{
	uint64_t End;

	if (!CheckedEnd(Base, Size, &End)) {
		return 0;
	}

	for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
		if ((Regions[Index].Flags & VmmRegionReserved) == 0) {
			continue;
		}

		uint64_t RegionEnd = Regions[Index].Base + Regions[Index].Size;
		if (Base >= Regions[Index].Base && End <= RegionEnd) {
			return &Regions[Index];
		}
	}

	return 0;
}

static void ZeroPage(uint64_t PhysicalAddress)
{
	uint8_t *Page = (uint8_t *)(uintptr_t)PmmPhysToHhdm(PhysicalAddress);
	if (Page == 0) {
		return;
	}

	for (uint64_t Index = 0; Index < PageSize; ++Index) {
		Page[Index] = 0;
	}
}

void VmmInit(void)
{
	for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
		Regions[Index].Base = 0;
		Regions[Index].Size = 0;
		Regions[Index].Committed = 0;
		Regions[Index].Flags = VmmRegionFree;
	}

	RegionCount = 0;
	LogInfo("VMM", "kernel arena 0x%llx-0x%llx",
			(unsigned long long)VmmKernelArenaBase,
			(unsigned long long)(VmmKernelArenaBase + VmmKernelArenaSize));
}

uint64_t VmmReserveRegion(uint64_t Size, uint64_t Flags)
{
	Size = AlignUp(Size, PageSize);
	if (Size == 0 || Size > VmmKernelArenaSize) {
		return 0;
	}

	for (uint64_t Base = VmmKernelArenaBase;
		 Base <= VmmKernelArenaBase + VmmKernelArenaSize - Size;
		 Base += PageSize) {
		bool Used = false;

		for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
			if ((Regions[Index].Flags & VmmRegionReserved) == 0) {
				continue;
			}

			if (RangesOverlap(Base, Size, Regions[Index].Base,
							  Regions[Index].Size)) {
				Used = true;
				Base = AlignUp(Regions[Index].Base + Regions[Index].Size,
							   PageSize) -
					   PageSize;
				break;
			}
		}

		if (Used) {
			continue;
		}

		VmmRegion *Region = AllocRegionSlot();
		if (Region == 0) {
			LogError("VMM", "out of region slots");
			return 0;
		}

		Region->Base = Base;
		Region->Size = Size;
		Region->Committed = 0;
		Region->Flags = Flags | VmmRegionReserved;
		++RegionCount;
		return Base;
	}

	return 0;
}

bool VmmCommitRange(uint64_t Base, uint64_t Size, uint64_t PagingFlags)
{
	uint64_t RequestedEnd;

	if (!CheckedEnd(Base, Size, &RequestedEnd)) {
		return false;
	}

	uint64_t Start = AlignDown(Base, PageSize);
	uint64_t End = AlignUp(RequestedEnd, PageSize);
	VmmRegion *Region = FindRegionForRange(Start, End - Start);

	if (Region == 0 || End < Start || End < RequestedEnd) {
		return false;
	}

	for (uint64_t Address = Start; Address < End; Address += PageSize) {
		if ((PagingGetFlags(Address) & PagingFlagPresent) != 0) {
			continue;
		}

		uint64_t PhysicalAddress = PmmAllocPageRawPhys();
		if (PhysicalAddress == 0) {
			VmmUncommitRange(Start, Address - Start);
			return false;
		}

		ZeroPage(PhysicalAddress);
		if (!PagingMapPage(Address, PhysicalAddress, PagingFlags)) {
			PmmFreePagePhys(PhysicalAddress);
			VmmUncommitRange(Start, Address - Start);
			return false;
		}
	}

	uint64_t NewCommitted = End - Region->Base;
	if (NewCommitted > Region->Committed) {
		Region->Committed = NewCommitted;
	}
	Region->Flags |= VmmRegionMapped | VmmRegionAnonymous;
	return true;
}

bool VmmUncommitRange(uint64_t Base, uint64_t Size)
{
	uint64_t RequestedEnd;

	if (Size == 0) {
		return true;
	}

	if (!CheckedEnd(Base, Size, &RequestedEnd)) {
		return false;
	}

	uint64_t Start = AlignDown(Base, PageSize);
	uint64_t End = AlignUp(RequestedEnd, PageSize);
	VmmRegion *Region = FindRegionForRange(Start, End - Start);

	if (Region == 0 || End < Start || End < RequestedEnd) {
		return false;
	}

	for (uint64_t Address = Start; Address < End; Address += PageSize) {
		uint64_t PhysicalAddress = PagingVirtToPhys(Address);
		if (PhysicalAddress != 0 &&
			(Region->Flags & VmmRegionAnonymous) != 0) {
			PmmFreePagePhys(PhysicalAddress);
		}
		PagingUnmapPage(Address);
	}

	if (Start <= Region->Base &&
		End >= Region->Base + Region->Committed) {
		Region->Committed = 0;
	} else if (End >= Region->Base + Region->Committed &&
			   Start >= Region->Base) {
		Region->Committed = Start - Region->Base;
	}

	if (Region->Committed == 0) {
		Region->Flags &= ~VmmRegionMapped;
	}

	return true;
}

uint64_t VmmMapAnonymous(uint64_t Size, uint64_t RegionFlags,
						 uint64_t PagingFlags)
{
	uint64_t VirtualBase =
		VmmReserveRegion(Size, RegionFlags | VmmRegionAnonymous);
	if (VirtualBase == 0) {
		return 0;
	}

	if (!VmmCommitRange(VirtualBase, Size, PagingFlags)) {
		VmmFreeRegion(VirtualBase);
		return 0;
	}

	return VirtualBase;
}

uint64_t VmmMapPhysicalRange(uint64_t PhysicalBase, uint64_t Size,
							 uint64_t RegionFlags, uint64_t PagingFlags)
{
	uint64_t VirtualBase =
		VmmReserveRegion(Size, RegionFlags | VmmRegionMapped);
	if (VirtualBase == 0) {
		return 0;
	}

	if (!PagingMapRange(VirtualBase, PhysicalBase, Size, PagingFlags)) {
		VmmFreeRegion(VirtualBase);
		return 0;
	}

	VmmRegion *Region = FindRegionByBase(VirtualBase);
	if (Region != 0) {
		Region->Committed = AlignUp(Size, PageSize);
	}

	return VirtualBase;
}

bool VmmUnmapRegion(uint64_t Base)
{
	return VmmFreeRegion(Base);
}

bool VmmFreeRegion(uint64_t Base)
{
	VmmRegion *Region = FindRegionByBase(Base);
	if (Region == 0) {
		return false;
	}

	if ((Region->Flags & VmmRegionMapped) != 0 || Region->Committed != 0) {
		VmmUncommitRange(Region->Base, Region->Size);
	}

	Region->Base = 0;
	Region->Size = 0;
	Region->Committed = 0;
	Region->Flags = VmmRegionFree;
	if (RegionCount > 0) {
		--RegionCount;
	}
	return true;
}

const VmmRegion *VmmFindRegion(uint64_t Address)
{
	for (uint64_t Index = 0; Index < VmmMaxRegions; ++Index) {
		if ((Regions[Index].Flags & VmmRegionReserved) == 0) {
			continue;
		}

		if (Address >= Regions[Index].Base &&
			Address < Regions[Index].Base + Regions[Index].Size) {
			return &Regions[Index];
		}
	}

	return 0;
}

bool VmmRangeMapped(uint64_t Base, uint64_t Size)
{
	uint64_t RequestedEnd;

	if (!CheckedEnd(Base, Size, &RequestedEnd)) {
		return false;
	}

	uint64_t Start = AlignDown(Base, PageSize);
	uint64_t End = AlignUp(RequestedEnd, PageSize);
	VmmRegion *Region = FindRegionForRange(Start, End - Start);

	if (Region == 0 || End < Start || End < RequestedEnd) {
		return false;
	}

	for (uint64_t Address = Start; Address < End; Address += PageSize) {
		if ((PagingGetFlags(Address) & PagingFlagPresent) == 0) {
			return false;
		}
	}

	return true;
}

uint64_t VmmGetRegionCount(void)
{
	return RegionCount;
}
