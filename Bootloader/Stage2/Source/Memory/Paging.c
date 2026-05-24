#include <Memory/Paging.h>

#include <stdbool.h>

#define PagePresent 0x001u
#define PageWritable 0x002u
#define PageWriteThrough 0x008u
#define PageCacheDisable 0x010u
#define PageHuge 0x080u
#define PageSize2M 0x200000ull
#define PageSize1G 0x40000000ull
#define PageTableEntries 512u
#define IdentityPdptEntries 4u
#define HhdmPdptEntries 4u
#define HigherHalfPdptIndex 510u
#define HigherHalfPml4Index 511u
#define HhdmPml4Index 256u
#define LowIdentityMappedSize PageSize1G

static uint64_t PageMapLevel4[PageTableEntries] __attribute__((aligned(4096)));
static uint64_t IdentityPageDirectoryPointerTable[PageTableEntries]
	__attribute__((aligned(4096)));
static uint64_t HigherHalfPageDirectoryPointerTable[PageTableEntries]
	__attribute__((aligned(4096)));
static uint64_t HhdmPageDirectoryPointerTable[PageTableEntries]
	__attribute__((aligned(4096)));

static uint64_t IdentityPageDirectories[IdentityPdptEntries][PageTableEntries]
	__attribute__((aligned(4096)));
static uint64_t HhdmPageDirectories[HhdmPdptEntries][PageTableEntries]
	__attribute__((aligned(4096)));
static uint64_t KernelPageDirectory[PageTableEntries]
	__attribute__((aligned(4096)));

static void ClearTable(uint64_t *Table)
{
	for (uint32_t Index = 0; Index < PageTableEntries; ++Index) {
		Table[Index] = 0;
	}
}

static uint64_t AlignDown2M(uint64_t Value)
{
	return Value & ~(PageSize2M - 1ull);
}

static uint64_t AlignUp2M(uint64_t Value)
{
	return (Value + PageSize2M - 1ull) & ~(PageSize2M - 1ull);
}

static bool RangeEnd(uint64_t Base, uint64_t Size, uint64_t *EndOut)
{
	if (Size == 0 || Base > UINT64_MAX - Size) {
		return false;
	}

	*EndOut = Base + Size;
	return true;
}

#define KernelPhysicalBase 0x400000ull

static void MapKernelHigherHalf1G(void)
{
	PageMapLevel4[HigherHalfPml4Index] =
		(uint32_t)HigherHalfPageDirectoryPointerTable | PagePresent |
		PageWritable;
	HigherHalfPageDirectoryPointerTable[HigherHalfPdptIndex] =
		(uint32_t)KernelPageDirectory | PagePresent | PageWritable;

	for (uint32_t Index = 0; Index < PageTableEntries; ++Index) {
		uint64_t PhysicalBase =
			KernelPhysicalBase + (uint64_t)Index * PageSize2M;
		KernelPageDirectory[Index] =
			PhysicalBase | PagePresent | PageWritable | PageHuge;
	}
}

static void MapHhdm2M(uint64_t PhysicalBase)
{
	uint32_t PdptIndex = (uint32_t)(PhysicalBase / PageSize1G);
	uint32_t PdIndex = (uint32_t)((PhysicalBase % PageSize1G) / PageSize2M);

	if (PdptIndex >= HhdmPdptEntries) {
		return;
	}

	HhdmPageDirectoryPointerTable[PdptIndex] =
		(uint32_t)HhdmPageDirectories[PdptIndex] | PagePresent | PageWritable;
	HhdmPageDirectories[PdptIndex][PdIndex] =
		PhysicalBase | PagePresent | PageWritable | PageHuge;
}

static void MapHhdmRange(uint64_t Base, uint64_t Size)
{
	uint64_t End;
	if (!RangeEnd(Base, Size, &End)) {
		return;
	}

	uint64_t Page = AlignDown2M(Base);
	uint64_t MappedEnd = AlignUp2M(End);

	for (; Page < MappedEnd; Page += PageSize2M) {
		MapHhdm2M(Page);
	}
}

static void MapIdentity2M(uint64_t PhysicalBase, uint64_t ExtraFlags)
{
	uint32_t PdptIndex = (uint32_t)(PhysicalBase / PageSize1G);
	uint32_t PdIndex = (uint32_t)((PhysicalBase % PageSize1G) / PageSize2M);

	if (PdptIndex >= IdentityPdptEntries) {
		return;
	}

	IdentityPageDirectoryPointerTable[PdptIndex] =
		(uint32_t)IdentityPageDirectories[PdptIndex] | PagePresent |
		PageWritable;
	IdentityPageDirectories[PdptIndex][PdIndex] =
		PhysicalBase | PagePresent | PageWritable | PageHuge | ExtraFlags;
}

static void MapIdentityRange(uint64_t Base, uint64_t Size, uint64_t ExtraFlags)
{
	uint64_t End;
	if (!RangeEnd(Base, Size, &End)) {
		return;
	}

	uint64_t Page = AlignDown2M(Base);
	uint64_t MappedEnd = AlignUp2M(End);

	for (; Page < MappedEnd; Page += PageSize2M) {
		MapIdentity2M(Page, ExtraFlags);
	}
}

static void MapFramebufferIdentity(const BootFramebuffer *Framebuffer)
{
	if (Framebuffer == 0 || Framebuffer->Address == 0 ||
		Framebuffer->Width == 0 || Framebuffer->Height == 0 ||
		Framebuffer->Pitch == 0) {
		return;
	}

	uint64_t Size = (uint64_t)Framebuffer->Pitch * Framebuffer->Height;
	MapIdentityRange(Framebuffer->Address, Size,
					 PageWriteThrough | PageCacheDisable);
}

uint32_t PagingBuildKernelMap(const BootFramebuffer *Framebuffer)
{
	ClearTable(PageMapLevel4);
	ClearTable(IdentityPageDirectoryPointerTable);
	ClearTable(HigherHalfPageDirectoryPointerTable);
	ClearTable(HhdmPageDirectoryPointerTable);
	ClearTable(KernelPageDirectory);

	for (uint32_t Index = 0; Index < IdentityPdptEntries; ++Index) {
		ClearTable(IdentityPageDirectories[Index]);
	}
	for (uint32_t Index = 0; Index < HhdmPdptEntries; ++Index) {
		ClearTable(HhdmPageDirectories[Index]);
	}

	PageMapLevel4[0] = (uint32_t)IdentityPageDirectoryPointerTable |
					   PagePresent | PageWritable;
	PageMapLevel4[HhdmPml4Index] =
		(uint32_t)HhdmPageDirectoryPointerTable | PagePresent | PageWritable;

	MapIdentityRange(0, LowIdentityMappedSize, 0);
	MapFramebufferIdentity(Framebuffer);

	MapHhdmRange(0, BootHhdmMappedSize);
	MapKernelHigherHalf1G();

	return (uint32_t)PageMapLevel4;
}

uint64_t PagingGetHhdmOffset(void)
{
	return BootHhdmOffset;
}

uint64_t PagingGetKernelPhysicalBase(void)
{
	return KernelPhysicalBase;
}
