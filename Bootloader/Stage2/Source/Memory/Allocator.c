#include <Memory/Allocator.h>

#include <Library/DebugLog.h>

#define AllocatorMinBase 0x00200000u
#define AllocatorDefaultAlignment 16u

extern uint8_t __stage2_end;

static uintptr_t HeapCursor;
static uintptr_t HeapBase;
static uintptr_t HeapEnd;

static uintptr_t AlignUp(uintptr_t Value, size_t Alignment)
{
	if (Alignment == 0) {
		Alignment = AllocatorDefaultAlignment;
	}

	uintptr_t Mask = (uintptr_t)Alignment - 1u;
	return (Value + Mask) & ~Mask;
}

static bool IsPowerOfTwo(size_t Value)
{
	return Value != 0 && (Value & (Value - 1u)) == 0;
}

bool AllocatorInit(const MemoryMap *Map)
{
	uintptr_t BestBase = 0;
	uintptr_t BestEnd = 0;
	uintptr_t BestLength = 0;
	uintptr_t Stage2End =
		AlignUp((uintptr_t)&__stage2_end, AllocatorDefaultAlignment);

	if (Map == 0) {
		BootError("ALLOC", "init rejected: missing memory map");
		return false;
	}

	for (uint32_t Index = 0;
		 Index < Map->EntriesCount && Index < MemoryMapMaxEntries; ++Index) {
		const MemoryMapEntry *Entry = &Map->Entries[Index];

		if (Entry->Type != MemoryMapTypeUsable || Entry->Length == 0 ||
			Entry->Base > UINT32_MAX) {
			continue;
		}

		uint64_t RangeEnd64 = Entry->Base + Entry->Length;
		if (RangeEnd64 > UINT32_MAX) {
			RangeEnd64 = UINT32_MAX;
		}

		uintptr_t Base = (uintptr_t)Entry->Base;
		uintptr_t End = (uintptr_t)RangeEnd64;

		if (Base < AllocatorMinBase) {
			Base = AllocatorMinBase;
		}

		if (Base < Stage2End && End > Stage2End) {
			Base = Stage2End;
		}

		Base = AlignUp(Base, AllocatorDefaultAlignment);

		if (End <= Base) {
			continue;
		}

		uintptr_t Length = End - Base;
		if (Length > BestLength) {
			BestBase = Base;
			BestEnd = End;
			BestLength = Length;
		}
	}

	if (BestBase == 0) {
		BootError("ALLOC", "no usable heap range found");
		return false;
	}

	HeapBase = BestBase;
	HeapCursor = BestBase;
	HeapEnd = BestEnd;
	DebugLog("ALLOC", "heap 0x%08x-0x%08x (%u KiB)", (unsigned int)HeapCursor,
			(unsigned int)HeapEnd,
			(unsigned int)((HeapEnd - HeapCursor) / 1024u));
	return true;
}

void *Alloc(size_t Size, size_t Alignment)
{
	if (Size == 0 || HeapCursor == 0) {
		return 0;
	}

	if (!IsPowerOfTwo(Alignment)) {
		Alignment = AllocatorDefaultAlignment;
	}

	uintptr_t Address = AlignUp(HeapCursor, Alignment);
	if (Address > HeapEnd || Size > (HeapEnd - Address)) {
		DebugLog("ALLOC", "out of memory allocating %u bytes",
				(unsigned int)Size);
		return 0;
	}

	HeapCursor = Address + Size;
	return (void *)Address;
}

void *Calloc(size_t ItemsCount, size_t Item)
{
	if (Item != 0 && ItemsCount > ((size_t)-1 / Item)) {
		return 0;
	}

	size_t Size = ItemsCount * Item;
	uint8_t *Memory = Alloc(Size, AllocatorDefaultAlignment);
	if (Memory == 0) {
		return 0;
	}

	for (size_t Index = 0; Index < Size; ++Index) {
		Memory[Index] = 0;
	}

	return Memory;
}

size_t AllocatorFreeBytes(void)
{
	if (HeapCursor == 0 || HeapEnd <= HeapCursor) {
		return 0;
	}

	return HeapEnd - HeapCursor;
}

bool AllocatorAllocatedRange(uintptr_t *BaseOut, uintptr_t *EndOut)
{
	if (HeapBase == 0 || HeapCursor <= HeapBase) {
		return false;
	}

	if (BaseOut != 0) {
		*BaseOut = HeapBase;
	}

	if (EndOut != 0) {
		*EndOut = HeapCursor;
	}

	return true;
}
