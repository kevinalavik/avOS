#include <Memory/Paging.h>

#include <Core/Log.h>
#include <Memory/Pmm.h>

#include <stddef.h>
#include <stdint.h>

#define KernelVirtualBase 0xffffffff80000000ull
#define PageTableEntries 512u

static uint64_t KernelPhysBase;
#define PageTableIndexMask 0x1ffull
#define PageAddressMask 0x000ffffffffff000ull
#define PageHuge1GAddressMask 0x000fffffc0000000ull
#define PageHuge2MAddressMask 0x000fffffffe00000ull
#define PageSize2M 0x200000ull
#define PageHuge 0x080ull
#define VgaTextAddress 0xB8000ull
#define VgaTextSize 0x1000ull

extern uint8_t __text_start;
extern uint8_t __text_end;
extern uint8_t __rodata_start;
extern uint8_t __rodata_end;
extern uint8_t __data_start;
extern uint8_t __bss_end;

static uint64_t RootTablePhys;

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static uint16_t Pml4Index(uint64_t Address)
{
	return (uint16_t)((Address >> 39) & PageTableIndexMask);
}

static uint16_t PdptIndex(uint64_t Address)
{
	return (uint16_t)((Address >> 30) & PageTableIndexMask);
}

static uint16_t PdIndex(uint64_t Address)
{
	return (uint16_t)((Address >> 21) & PageTableIndexMask);
}

static uint16_t PtIndex(uint64_t Address)
{
	return (uint16_t)((Address >> 12) & PageTableIndexMask);
}

static void InvalidatePage(uint64_t Address)
{
	__asm__ volatile("invlpg (%0)" ::"r"(Address) : "memory");
}

static uint64_t KernelVirtToPhys(uint64_t Address)
{
	return Address - KernelVirtualBase + KernelPhysBase;
}

static uint64_t *TableFromPhys(uint64_t PhysicalAddress)
{
	return (uint64_t *)(uintptr_t)PmmPhysToHhdm(PhysicalAddress);
}

static void ClearTable(uint64_t *Table)
{
	for (uint32_t Index = 0; Index < PageTableEntries; ++Index) {
		Table[Index] = 0;
	}
}

static uint64_t AllocTable(void)
{
	uint64_t PhysicalAddress = PmmAllocPageRawPhys();
	if (PhysicalAddress == 0) {
		return 0;
	}

	uint64_t *Table = TableFromPhys(PhysicalAddress);
	if (Table == 0) {
		return 0;
	}

	ClearTable(Table);
	return PhysicalAddress;
}

static bool GetOrCreateTable(uint64_t *Parent, uint16_t Index, uint64_t Flags,
							 uint64_t *PhysicalOut)
{
	if ((Parent[Index] & PagingFlagPresent) == 0) {
		uint64_t PhysicalAddress = AllocTable();
		if (PhysicalAddress == 0) {
			return false;
		}

		Parent[Index] =
			PhysicalAddress | PagingFlagPresent | PagingFlagWritable;
	}

	if ((Flags & PagingFlagUser) != 0) {
		Parent[Index] |= PagingFlagUser;
	}

	*PhysicalOut = Parent[Index] & ~0xfffull;
	return true;
}

bool PagingMapPage(uint64_t VirtualAddress, uint64_t PhysicalAddress,
				   uint64_t Flags)
{
	uint64_t *Pml4 = TableFromPhys(RootTablePhys);
	uint64_t PdptPhys;
	uint64_t PdPhys;
	uint64_t PtPhys;

	if (Pml4 == 0) {
		return false;
	}

	if (!GetOrCreateTable(Pml4, Pml4Index(VirtualAddress), Flags, &PdptPhys)) {
		return false;
	}

	uint64_t *Pdpt = TableFromPhys(PdptPhys);
	if (!GetOrCreateTable(Pdpt, PdptIndex(VirtualAddress), Flags, &PdPhys)) {
		return false;
	}

	uint64_t *Pd = TableFromPhys(PdPhys);
	if (!GetOrCreateTable(Pd, PdIndex(VirtualAddress), Flags, &PtPhys)) {
		return false;
	}

	uint64_t *Pt = TableFromPhys(PtPhys);
	Pt[PtIndex(VirtualAddress)] =
		AlignDown(PhysicalAddress, PageSize) | Flags | PagingFlagPresent;
	return true;
}

static bool PagingMapHuge2M(uint64_t VirtualAddress, uint64_t PhysicalAddress,
							uint64_t Flags)
{
	uint64_t *Pml4 = TableFromPhys(RootTablePhys);
	uint64_t PdptPhys;
	uint64_t PdPhys;

	if (Pml4 == 0) {
		return false;
	}

	if (!GetOrCreateTable(Pml4, Pml4Index(VirtualAddress), Flags, &PdptPhys)) {
		return false;
	}

	uint64_t *Pdpt = TableFromPhys(PdptPhys);
	if (!GetOrCreateTable(Pdpt, PdptIndex(VirtualAddress), Flags, &PdPhys)) {
		return false;
	}

	uint64_t *Pd = TableFromPhys(PdPhys);
	Pd[PdIndex(VirtualAddress)] = AlignDown(PhysicalAddress, PageSize2M) |
								  Flags | PagingFlagPresent | PageHuge;
	return true;
}

bool PagingMapRange(uint64_t VirtualBase, uint64_t PhysicalBase, uint64_t Size,
					uint64_t Flags)
{
	if (Size == 0) {
		return true;
	}

	uint64_t Offset = 0;
	uint64_t StartOffset = PhysicalBase & (PageSize - 1ull);
	uint64_t MappedSize = AlignUp(Size + StartOffset, PageSize);
	uint64_t Virtual = AlignDown(VirtualBase, PageSize);
	uint64_t Physical = AlignDown(PhysicalBase, PageSize);

	while (Offset < MappedSize) {
		if (!PagingMapPage(Virtual + Offset, Physical + Offset, Flags)) {
			return false;
		}
		Offset += PageSize;
	}

	return true;
}

void PagingUnmapPage(uint64_t VirtualAddress)
{
	uint64_t *Pml4 = TableFromPhys(RootTablePhys);
	uint64_t Entry;

	if (Pml4 == 0) {
		return;
	}

	Entry = Pml4[Pml4Index(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return;
	}

	uint64_t *Pdpt = TableFromPhys(Entry & ~0xfffull);
	Entry = Pdpt[PdptIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0 || (Entry & PageHuge) != 0) {
		return;
	}

	uint64_t *Pd = TableFromPhys(Entry & ~0xfffull);
	Entry = Pd[PdIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0 || (Entry & PageHuge) != 0) {
		return;
	}

	uint64_t *Pt = TableFromPhys(Entry & ~0xfffull);
	Pt[PtIndex(VirtualAddress)] = 0;
	InvalidatePage(VirtualAddress);
}

void PagingUnmapRange(uint64_t VirtualBase, uint64_t Size)
{
	uint64_t StartOffset = VirtualBase & (PageSize - 1ull);
	uint64_t MappedSize = AlignUp(Size + StartOffset, PageSize);
	uint64_t Virtual = AlignDown(VirtualBase, PageSize);

	for (uint64_t Offset = 0; Offset < MappedSize; Offset += PageSize) {
		PagingUnmapPage(Virtual + Offset);
	}
}

uint64_t PagingVirtToPhys(uint64_t VirtualAddress)
{
	uint64_t *Pml4 = TableFromPhys(RootTablePhys);
	uint64_t Entry;

	if (Pml4 == 0) {
		return 0;
	}

	Entry = Pml4[Pml4Index(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}

	uint64_t *Pdpt = TableFromPhys(Entry & ~0xfffull);
	Entry = Pdpt[PdptIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}
	if ((Entry & PageHuge) != 0) {
		return (Entry & PageHuge1GAddressMask) |
			   (VirtualAddress & 0x3fffffffull);
	}

	uint64_t *Pd = TableFromPhys(Entry & ~0xfffull);
	Entry = Pd[PdIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}
	if ((Entry & PageHuge) != 0) {
		return (Entry & PageHuge2MAddressMask) | (VirtualAddress & 0x1fffffull);
	}

	uint64_t *Pt = TableFromPhys(Entry & ~0xfffull);
	Entry = Pt[PtIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}

	return (Entry & PageAddressMask) | (VirtualAddress & (PageSize - 1ull));
}

uint64_t PagingGetFlags(uint64_t VirtualAddress)
{
	uint64_t *Pml4 = TableFromPhys(RootTablePhys);
	uint64_t Entry;

	if (Pml4 == 0) {
		return 0;
	}

	Entry = Pml4[Pml4Index(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}

	uint64_t *Pdpt = TableFromPhys(Entry & ~0xfffull);
	Entry = Pdpt[PdptIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}
	if ((Entry & PageHuge) != 0) {
		return Entry &
			   (PagingFlagPresent | PagingFlagWritable | PagingFlagUser |
				PagingFlagGlobal | PagingFlagNoExecute);
	}

	uint64_t *Pd = TableFromPhys(Entry & ~0xfffull);
	Entry = Pd[PdIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}
	if ((Entry & PageHuge) != 0) {
		return Entry &
			   (PagingFlagPresent | PagingFlagWritable | PagingFlagUser |
				PagingFlagGlobal | PagingFlagNoExecute);
	}

	uint64_t *Pt = TableFromPhys(Entry & ~0xfffull);
	Entry = Pt[PtIndex(VirtualAddress)];
	if ((Entry & PagingFlagPresent) == 0) {
		return 0;
	}

	return Entry & (PagingFlagPresent | PagingFlagWritable | PagingFlagUser |
					PagingFlagGlobal | PagingFlagNoExecute);
}

static bool PagingMapHugeRange(uint64_t VirtualBase, uint64_t PhysicalBase,
							   uint64_t Size, uint64_t Flags)
{
	uint64_t Virtual = AlignDown(VirtualBase, PageSize2M);
	uint64_t Physical = AlignDown(PhysicalBase, PageSize2M);
	uint64_t End = AlignUp(PhysicalBase + Size, PageSize2M);

	for (; Physical < End; Physical += PageSize2M, Virtual += PageSize2M) {
		if (!PagingMapHuge2M(Virtual, Physical, Flags)) {
			return false;
		}
	}

	return true;
}

static bool MapKernel(void)
{
	uint64_t TextStart = (uint64_t)(uintptr_t)&__text_start;
	uint64_t TextEnd = (uint64_t)(uintptr_t)&__text_end;
	uint64_t RodataStart = (uint64_t)(uintptr_t)&__rodata_start;
	uint64_t RodataEnd = (uint64_t)(uintptr_t)&__rodata_end;
	uint64_t DataStart = (uint64_t)(uintptr_t)&__data_start;
	uint64_t BssEnd = (uint64_t)(uintptr_t)&__bss_end;

	if (!PagingMapRange(TextStart, KernelVirtToPhys(TextStart),
						TextEnd - TextStart, 0)) {
		return false;
	}

	if (!PagingMapRange(RodataStart, KernelVirtToPhys(RodataStart),
						RodataEnd - RodataStart, 0)) {
		return false;
	}

	return PagingMapRange(DataStart, KernelVirtToPhys(DataStart),
						  BssEnd - DataStart, PagingFlagWritable);
}

static bool MapHhdm(void)
{
	return PagingMapHugeRange(BootHhdmOffset, 0, BootHhdmMappedSize,
							  PagingFlagWritable);
}

static bool MapFramebuffer(const BootFramebuffer *Framebuffer)
{
	if (Framebuffer == 0 || Framebuffer->Address == 0 ||
		Framebuffer->Width == 0 || Framebuffer->Height == 0 ||
		Framebuffer->Pitch == 0) {
		return PagingMapRange(VgaTextAddress, VgaTextAddress, VgaTextSize,
							  PagingFlagWritable | PagingFlagWriteThrough |
								  PagingFlagCacheDisable);
	}

	uint64_t Size = (uint64_t)Framebuffer->Pitch * Framebuffer->Height;
	return PagingMapRange(Framebuffer->Address, Framebuffer->Address, Size,
						  PagingFlagWritable | PagingFlagWriteThrough |
							  PagingFlagCacheDisable);
}

uint64_t PagingBuild(const BootInfo *Info)
{
	KernelPhysBase = Info->KernelPhysicalBase;

	RootTablePhys = AllocTable();
	if (RootTablePhys == 0) {
		LogError("core.mm.paging", "failed to allocate root table");
		return 0;
	}

	if (!MapKernel() || !MapHhdm() || !MapFramebuffer(&Info->Framebuffer)) {
		LogError("core.mm.paging", "failed to build kernel page tables");
		return 0;
	}

	LogTrace("core.mm.paging", "loading cr3=0x%llx",
			 (unsigned long long)RootTablePhys);
	return RootTablePhys;
}

uint64_t PagingGetRootTablePhys(void)
{
	return RootTablePhys;
}
