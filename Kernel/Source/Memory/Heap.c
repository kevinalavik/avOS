#include <Memory/Heap.h>

#include <Core/Log.h>
#include <Memory/PageDb.h>
#include <Memory/Paging.h>
#include <Memory/Vmm.h>

#include <stddef.h>
#include <stdint.h>

#define HeapBlockMagic 0x48454150424c4b31ull
#define HeapBlockAlignment 16ull
#define HeapMinPayload 16ull

typedef struct HeapBlock {
	uint64_t Magic;
	uint64_t Size;
	struct HeapBlock *Prev;
	struct HeapBlock *Next;
	bool Free;
	uint8_t Reserved[7];
} HeapBlock;

static uint64_t HeapBase;
static uint64_t HeapCommitted;
static uint64_t HeapReserved;
static uint64_t HeapAllocatedBytes;
static uint64_t HeapAllocCount;
static uint64_t HeapFreeCount;
static HeapBlock *HeapHead;

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	if (Alignment == 0) {
		Alignment = sizeof(uintptr_t);
	}

	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static bool IsPowerOfTwo(uint64_t Value)
{
	return Value != 0 && (Value & (Value - 1ull)) == 0;
}

static uint64_t HeaderSize(void)
{
	return AlignUp(sizeof(HeapBlock), HeapBlockAlignment);
}

static uint64_t MinBlockTotal(void)
{
	return HeaderSize() + HeapMinPayload;
}

static uint8_t *BlockData(HeapBlock *Block)
{
	return (uint8_t *)Block + HeaderSize();
}

static uint64_t BlockEnd(const HeapBlock *Block)
{
	return (uint64_t)(uintptr_t)Block + HeaderSize() + Block->Size;
}

static void InitBlock(HeapBlock *Block, uint64_t Size, bool Free)
{
	Block->Magic = HeapBlockMagic;
	Block->Size = Size;
	Block->Prev = 0;
	Block->Next = 0;
	Block->Free = Free;
	for (uint64_t Index = 0; Index < sizeof(Block->Reserved); ++Index) {
		Block->Reserved[Index] = 0;
	}
}

static void InsertAfter(HeapBlock *Existing, HeapBlock *Block)
{
	Block->Prev = Existing;
	Block->Next = Existing->Next;
	if (Existing->Next != 0) {
		Existing->Next->Prev = Block;
	}
	Existing->Next = Block;
}

static void AppendBlock(HeapBlock *Block)
{
	if (HeapHead == 0) {
		HeapHead = Block;
		return;
	}

	HeapBlock *Tail = HeapHead;
	while (Tail->Next != 0) {
		Tail = Tail->Next;
	}

	Tail->Next = Block;
	Block->Prev = Tail;
}

static void RemoveBlock(HeapBlock *Block)
{
	if (Block->Prev != 0) {
		Block->Prev->Next = Block->Next;
	} else {
		HeapHead = Block->Next;
	}

	if (Block->Next != 0) {
		Block->Next->Prev = Block->Prev;
	}

	Block->Prev = 0;
	Block->Next = 0;
}

static HeapBlock *FindTail(void)
{
	HeapBlock *Tail = HeapHead;
	if (Tail == 0) {
		return 0;
	}

	while (Tail->Next != 0) {
		Tail = Tail->Next;
	}

	return Tail;
}

static uint64_t RecomputeFreeBytes(void)
{
	uint64_t FreeBytes = 0;

	for (HeapBlock *Block = HeapHead; Block != 0; Block = Block->Next) {
		if (Block->Magic == HeapBlockMagic && Block->Free) {
			FreeBytes += Block->Size;
		}
	}

	return FreeBytes;
}

static bool HeapGrow(uint64_t MinAdditionalBytes)
{
	uint64_t GrowBytes = KernelHeapInitialPages * PageSize;
	if (GrowBytes < MinAdditionalBytes) {
		GrowBytes = MinAdditionalBytes;
	}

	if (GrowBytes > UINT64_MAX - HeapCommitted) {
		return false;
	}

	uint64_t NewCommitted = AlignUp(HeapCommitted + GrowBytes, PageSize);
	if (NewCommitted <= HeapCommitted || NewCommitted > HeapReserved) {
		return false;
	}

	uint64_t OldCommitted = HeapCommitted;
	uint64_t CommitBytes = NewCommitted - OldCommitted;
	if (!VmmCommitRange(HeapBase + OldCommitted, CommitBytes,
						PagingFlagWritable)) {
		return false;
	}

	HeapCommitted = NewCommitted;

	uint64_t NewBlockAddress = HeapBase + OldCommitted;
	HeapBlock *Tail = FindTail();
	if (Tail != 0 && Tail->Free && BlockEnd(Tail) == NewBlockAddress) {
		Tail->Size += CommitBytes;
		return true;
	}

	if (CommitBytes < MinBlockTotal()) {
		return true;
	}

	HeapBlock *Block = (HeapBlock *)(uintptr_t)NewBlockAddress;
	InitBlock(Block, CommitBytes - HeaderSize(), true);
	AppendBlock(Block);
	return true;
}

static HeapBlock *FindFreeBlock(size_t Size, size_t Alignment)
{
	uint64_t PayloadSize = AlignUp(Size, HeapBlockAlignment);

	for (HeapBlock *Block = HeapHead; Block != 0; Block = Block->Next) {
		if (Block->Magic != HeapBlockMagic || !Block->Free) {
			continue;
		}

		uint64_t BlockBase = (uint64_t)(uintptr_t)Block;
		uint64_t End = BlockEnd(Block);
		uint64_t HeaderAddress =
			AlignUp(BlockBase + HeaderSize(), Alignment) - HeaderSize();

		while (HeaderAddress > BlockBase &&
			   HeaderAddress - BlockBase < MinBlockTotal()) {
			if (HeaderAddress > UINT64_MAX - Alignment) {
				HeaderAddress = UINT64_MAX;
				break;
			}
			HeaderAddress += Alignment;
		}

		if (HeaderAddress < BlockBase ||
			HeaderAddress > UINT64_MAX - HeaderSize() ||
			HeaderAddress + HeaderSize() > UINT64_MAX - PayloadSize) {
			continue;
		}

		if (HeaderAddress + HeaderSize() + PayloadSize <= End) {
			return Block;
		}
	}

	return 0;
}

static HeapBlock *CarveBlock(HeapBlock *Block, size_t Size, size_t Alignment)
{
	uint64_t PayloadSize = AlignUp(Size, HeapBlockAlignment);
	uint64_t BlockBase = (uint64_t)(uintptr_t)Block;
	uint64_t End = BlockEnd(Block);
	uint64_t HeaderAddress =
		AlignUp(BlockBase + HeaderSize(), Alignment) - HeaderSize();

	while (HeaderAddress > BlockBase &&
		   HeaderAddress - BlockBase < MinBlockTotal()) {
		if (HeaderAddress > UINT64_MAX - Alignment) {
			return 0;
		}
		HeaderAddress += Alignment;
	}

	if (HeaderAddress > BlockBase) {
		uint64_t PrefixTotal = HeaderAddress - BlockBase;
		Block->Size = PrefixTotal - HeaderSize();

		HeapBlock *AlignedBlock = (HeapBlock *)(uintptr_t)HeaderAddress;
		InitBlock(AlignedBlock, End - HeaderAddress - HeaderSize(), true);
		InsertAfter(Block, AlignedBlock);
		Block = AlignedBlock;
	}

	uint64_t AllocationEnd = (uint64_t)(uintptr_t)Block + HeaderSize() +
							 PayloadSize;
	if (End >= AllocationEnd + MinBlockTotal()) {
		HeapBlock *Suffix = (HeapBlock *)(uintptr_t)AllocationEnd;
		InitBlock(Suffix, End - AllocationEnd - HeaderSize(), true);
		InsertAfter(Block, Suffix);
		Block->Size = PayloadSize;
	} else {
		Block->Size = End - (uint64_t)(uintptr_t)Block - HeaderSize();
	}

	Block->Free = false;
	return Block;
}

static HeapBlock *PointerToBlock(void *Pointer)
{
	uint64_t Address = (uint64_t)(uintptr_t)Pointer;
	if (Pointer == 0 || Address < HeapBase + HeaderSize() ||
		Address >= HeapBase + HeapCommitted) {
		return 0;
	}

	HeapBlock *Block =
		(HeapBlock *)(uintptr_t)(Address - HeaderSize());
	if (Block->Magic != HeapBlockMagic) {
		return 0;
	}

	return Block;
}

static void Coalesce(HeapBlock *Block)
{
	if (Block->Next != 0 && Block->Next->Free &&
		BlockEnd(Block) == (uint64_t)(uintptr_t)Block->Next) {
		HeapBlock *Next = Block->Next;
		Block->Size = BlockEnd(Next) - (uint64_t)(uintptr_t)Block -
					  HeaderSize();
		RemoveBlock(Next);
	}

	if (Block->Prev != 0 && Block->Prev->Free &&
		BlockEnd(Block->Prev) == (uint64_t)(uintptr_t)Block) {
		HeapBlock *Prev = Block->Prev;
		Prev->Size = BlockEnd(Block) - (uint64_t)(uintptr_t)Prev -
					 HeaderSize();
		RemoveBlock(Block);
	}
}

bool HeapInit(void)
{
	HeapReserved = KernelHeapReserveSize;
	HeapBase = VmmReserveRegion(HeapReserved, VmmRegionKernelHeap |
												   VmmRegionWritable |
												   VmmRegionAnonymous);
	if (HeapBase == 0) {
		LogError("HEAP", "failed to reserve kernel heap");
		return false;
	}

	HeapHead = 0;
	HeapCommitted = 0;
	HeapAllocatedBytes = 0;
	HeapAllocCount = 0;
	HeapFreeCount = 0;

	if (!HeapGrow(KernelHeapInitialPages * PageSize)) {
		LogError("HEAP", "failed to commit initial heap pages");
		return false;
	}

	LogInfo("HEAP", "base=0x%llx reserved=%llu MiB committed=%llu KiB",
			(unsigned long long)HeapBase,
			(unsigned long long)(HeapReserved >> 20),
			(unsigned long long)(HeapCommitted >> 10));
	return true;
}

void *KernelAllocAligned(size_t Size, size_t Alignment)
{
	if (Size == 0 || HeapBase == 0) {
		return 0;
	}

	if (!IsPowerOfTwo((uint64_t)Alignment)) {
		Alignment = HeapBlockAlignment;
	}
	if (Alignment < HeapBlockAlignment) {
		Alignment = HeapBlockAlignment;
	}

	if (Size > UINT64_MAX - HeaderSize() - Alignment) {
		return 0;
	}

	uint64_t PayloadSize = AlignUp(Size, HeapBlockAlignment);
	uint64_t NeedBytes = HeaderSize() + PayloadSize + Alignment;

	HeapBlock *Block = FindFreeBlock(Size, Alignment);
	while (Block == 0) {
		if (!HeapGrow(NeedBytes)) {
			LogWarn("HEAP", "out of memory allocating %llu bytes",
					(unsigned long long)Size);
			return 0;
		}
		Block = FindFreeBlock(Size, Alignment);
	}

	Block = CarveBlock(Block, Size, Alignment);
	HeapAllocatedBytes += Block->Size;
	++HeapAllocCount;
	return BlockData(Block);
}

void *KernelAlloc(size_t Size)
{
	return KernelAllocAligned(Size, HeapBlockAlignment);
}

void *KernelZalloc(size_t Size)
{
	uint8_t *Memory = KernelAlloc(Size);
	if (Memory == 0) {
		return 0;
	}

	for (size_t Index = 0; Index < Size; ++Index) {
		Memory[Index] = 0;
	}

	return Memory;
}

void *KernelCalloc(size_t ItemsCount, size_t ItemSize)
{
	if (ItemSize != 0 && ItemsCount > ((size_t)-1 / ItemSize)) {
		return 0;
	}

	return KernelZalloc(ItemsCount * ItemSize);
}

void KernelFree(void *Pointer)
{
	HeapBlock *Block = PointerToBlock(Pointer);
	if (Block == 0) {
		LogWarn("HEAP", "refusing invalid free ptr=0x%llx",
				(unsigned long long)(uintptr_t)Pointer);
		return;
	}

	if (Block->Free) {
		LogWarn("HEAP", "refusing double free ptr=0x%llx",
				(unsigned long long)(uintptr_t)Pointer);
		return;
	}

	Block->Free = true;
	if (HeapAllocatedBytes >= Block->Size) {
		HeapAllocatedBytes -= Block->Size;
	} else {
		HeapAllocatedBytes = 0;
	}
	++HeapFreeCount;
	Coalesce(Block);
}

void *KernelRealloc(void *Pointer, size_t Size)
{
	return HeapRealloc(Pointer, Size, HeapBlockAlignment);
}

void *HeapAlloc(size_t Size, size_t Alignment)
{
	return KernelAllocAligned(Size, Alignment);
}

void *HeapCalloc(size_t ItemsCount, size_t ItemSize, size_t Alignment)
{
	if (ItemSize != 0 && ItemsCount > ((size_t)-1 / ItemSize)) {
		return 0;
	}

	size_t Size = ItemsCount * ItemSize;
	uint8_t *Memory = HeapAlloc(Size, Alignment);
	if (Memory == 0) {
		return 0;
	}

	for (size_t Index = 0; Index < Size; ++Index) {
		Memory[Index] = 0;
	}

	return Memory;
}

void *HeapRealloc(void *Pointer, size_t Size, size_t Alignment)
{
	if (Pointer == 0) {
		return HeapAlloc(Size, Alignment);
	}

	if (Size == 0) {
		HeapFree(Pointer);
		return 0;
	}

	if (!IsPowerOfTwo((uint64_t)Alignment) || Alignment < HeapBlockAlignment) {
		Alignment = HeapBlockAlignment;
	}

	HeapBlock *Block = PointerToBlock(Pointer);
	if (Block == 0 || Block->Free) {
		return 0;
	}

	uint64_t Address = (uint64_t)(uintptr_t)Pointer;
	uint64_t PayloadSize = AlignUp(Size, HeapBlockAlignment);
	if ((Address & (Alignment - 1ull)) == 0 && Block->Size >= PayloadSize) {
		uint64_t OldSize = Block->Size;
		uint64_t End = BlockEnd(Block);
		uint64_t NewEnd = (uint64_t)(uintptr_t)Block + HeaderSize() +
						  PayloadSize;

		if (End >= NewEnd + MinBlockTotal()) {
			HeapBlock *Suffix = (HeapBlock *)(uintptr_t)NewEnd;
			InitBlock(Suffix, End - NewEnd - HeaderSize(), true);
			InsertAfter(Block, Suffix);
			Block->Size = PayloadSize;
			if (HeapAllocatedBytes >= OldSize - Block->Size) {
				HeapAllocatedBytes -= OldSize - Block->Size;
			}
			Coalesce(Suffix);
		}
		return Pointer;
	}

	void *NewPointer = HeapAlloc(Size, Alignment);
	if (NewPointer == 0) {
		return 0;
	}

	size_t CopySize = Block->Size < Size ? (size_t)Block->Size : Size;
	uint8_t *Destination = (uint8_t *)NewPointer;
	uint8_t *Source = (uint8_t *)Pointer;
	for (size_t Index = 0; Index < CopySize; ++Index) {
		Destination[Index] = Source[Index];
	}

	HeapFree(Pointer);
	return NewPointer;
}

void HeapFree(void *Pointer)
{
	KernelFree(Pointer);
}

uint64_t HeapGetBase(void)
{
	return HeapBase;
}

uint64_t HeapGetUsedBytes(void)
{
	return HeapAllocatedBytes;
}

uint64_t HeapGetFreeBytes(void)
{
	return RecomputeFreeBytes();
}

uint64_t HeapGetCommittedBytes(void)
{
	return HeapCommitted;
}

uint64_t HeapGetReservedBytes(void)
{
	return HeapReserved;
}

uint64_t HeapGetAllocCount(void)
{
	return HeapAllocCount;
}

uint64_t HeapGetFreeCount(void)
{
	return HeapFreeCount;
}
