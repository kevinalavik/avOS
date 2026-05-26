#include <System/Memory.h>
#include <System/Syscall.h>

#define PAGE_SIZE 4096ull
#define ALIGNMENT 16ull
#define HEAP_CHUNK_SIZE (1024ull * 64)

typedef struct MemBlock {
	Size Size;
	Size Pad;
} MemBlock;

typedef struct FreeBlock {
	struct FreeBlock *Next;
	struct FreeBlock *Prev;
} FreeBlock;

typedef struct BlockFooter {
	Size Size;
} BlockFooter;

typedef struct HeapRegion {
	struct HeapRegion *Next;
	void *End;
} HeapRegion;

#define BLOCK_OVERHEAD (sizeof(MemBlock) + sizeof(BlockFooter))
#define MIN_BLOCK_SIZE \
	((BLOCK_OVERHEAD + sizeof(FreeBlock) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))

#define SIZE_MASK (~(Size)1)
#define FREE_FLAG 1
#define SIZE_IS_FREE(s) ((s) & FREE_FLAG)
#define SIZE_BLOCK_SZ(s) ((s) & SIZE_MASK)

#define BLOCK_SIZE(b) ((b)->Size & SIZE_MASK)
#define IS_FREE(b) ((b)->Size & FREE_FLAG)
#define SET_FREE(b) ((b)->Size |= FREE_FLAG)
#define SET_ALLOC(b) ((b)->Size &= ~FREE_FLAG)
#define NEXT_BLOCK(b) ((MemBlock *)((Byte *)(b) + BLOCK_SIZE(b)))
#define FOOTER(b) \
	((BlockFooter *)((Byte *)(b) + BLOCK_SIZE(b) - sizeof(BlockFooter)))
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((Size)(a) - 1))

static FreeBlock *FreeList = 0;
static HeapRegion *RegionList = 0;

static void FreeListRemove(FreeBlock *Fb)
{
	if (Fb->Prev)
		Fb->Prev->Next = Fb->Next;
	else
		FreeList = Fb->Next;
	if (Fb->Next)
		Fb->Next->Prev = Fb->Prev;
}

static void FreeListInsert(FreeBlock *Fb)
{
	Fb->Next = FreeList;
	Fb->Prev = 0;
	if (FreeList)
		FreeList->Prev = Fb;
	FreeList = Fb;
}

static HeapRegion *FindRegion(void *Ptr)
{
	HeapRegion *Reg = RegionList;
	while (Reg) {
		if ((Byte *)Ptr >= (Byte *)Reg + sizeof(HeapRegion) &&
			(Byte *)Ptr < (Byte *)Reg->End) {
			return Reg;
		}
		Reg = Reg->Next;
	}
	return 0;
}

static int ExpandHeap(Size MinSize)
{
	Size ChunkSize = ALIGN_UP(HEAP_CHUNK_SIZE, PAGE_SIZE);
	while (ChunkSize < MinSize + sizeof(HeapRegion))
		ChunkSize += PAGE_SIZE;

	void *Base = (void *)Syscall2(SyscallMap, ChunkSize, 1);
	if (Base == (void *)(U64)-1 || Base == 0)
		return 0;

	HeapRegion *Reg = (HeapRegion *)Base;
	Reg->Next = RegionList;
	Reg->End = (Byte *)Base + ChunkSize;
	RegionList = Reg;

	MemBlock *Block = (MemBlock *)((Byte *)Base + sizeof(HeapRegion));
	Size Available = ChunkSize - sizeof(HeapRegion);
	Block->Size = Available | FREE_FLAG;
	Block->Pad = 0;
	FOOTER(Block)->Size = Block->Size;

	FreeBlock *Fb = (FreeBlock *)(Block + 1);
	FreeListInsert(Fb);

	return 1;
}

void *MemoryAlloc(Size Bytes)
{
	if (Bytes == 0)
		return 0;

	Size Needed = ALIGN_UP(Bytes + BLOCK_OVERHEAD, ALIGNMENT);
	if (Needed < MIN_BLOCK_SIZE)
		Needed = MIN_BLOCK_SIZE;

	while (1) {
		FreeBlock *Fb = FreeList;
		while (Fb) {
			MemBlock *Block = (MemBlock *)Fb - 1;
			Size BlockSz = BLOCK_SIZE(Block);

			if (BlockSz >= Needed) {
				FreeListRemove(Fb);

				Size Remainder = BlockSz - Needed;
				if (Remainder >= MIN_BLOCK_SIZE) {
					Block->Size = Needed;
					Block->Pad = 0;
					FOOTER(Block)->Size = Needed;

					MemBlock *NewBlock = NEXT_BLOCK(Block);
					NewBlock->Size = Remainder | FREE_FLAG;
					NewBlock->Pad = 0;
					FOOTER(NewBlock)->Size = NewBlock->Size;

					FreeListInsert((FreeBlock *)(NewBlock + 1));
				} else {
					Block->Size = BlockSz;
					FOOTER(Block)->Size = BlockSz;
				}

				return (void *)(Block + 1);
			}

			Fb = Fb->Next;
		}

		if (!ExpandHeap(Needed))
			return 0;
	}
}

void *MemoryRealloc(void *Pointer, Size Bytes)
{
	if (Pointer == 0)
		return MemoryAlloc(Bytes);
	if (Bytes == 0) {
		MemoryFree(Pointer);
		return 0;
	}

	MemBlock *Block = (MemBlock *)Pointer - 1;
	Size OldDataSize = BLOCK_SIZE(Block) - BLOCK_OVERHEAD;

	if (Bytes <= OldDataSize)
		return Pointer;

	HeapRegion *Reg = FindRegion(Block);
	MemBlock *Next = NEXT_BLOCK(Block);
	if (Reg && (Byte *)Next < (Byte *)Reg->End && IS_FREE(Next)) {
		Size Needed = ALIGN_UP(Bytes + BLOCK_OVERHEAD, ALIGNMENT);
		if (Needed < MIN_BLOCK_SIZE)
			Needed = MIN_BLOCK_SIZE;

		Size Combined = BLOCK_SIZE(Block) + BLOCK_SIZE(Next);
		if (Combined >= Needed) {
			FreeListRemove((FreeBlock *)(Next + 1));

			Size Remainder = Combined - Needed;
			if (Remainder >= MIN_BLOCK_SIZE) {
				Block->Size = Needed;
				FOOTER(Block)->Size = Needed;

				MemBlock *NewBlock = NEXT_BLOCK(Block);
				NewBlock->Size = Remainder | FREE_FLAG;
				NewBlock->Pad = 0;
				FOOTER(NewBlock)->Size = NewBlock->Size;

				FreeListInsert((FreeBlock *)(NewBlock + 1));
			} else {
				Block->Size = Combined;
				FOOTER(Block)->Size = Combined;
			}

			return (void *)(Block + 1);
		}
	}

	void *NewPtr = MemoryAlloc(Bytes);
	if (NewPtr) {
		Byte *Src = (Byte *)Pointer;
		Byte *Dst = (Byte *)NewPtr;
		Size CopySize = OldDataSize < Bytes ? OldDataSize : Bytes;
		for (Size i = 0; i < CopySize; i++)
			Dst[i] = Src[i];
		MemoryFree(Pointer);
	}
	return NewPtr;
}

void *MemoryCalloc(Size Num, Size ElemSize)
{
	if (Num == 0 || ElemSize == 0)
		return 0;

	Size Total = Num * ElemSize;
	if (Num != Total / ElemSize)
		return 0;

	void *Ptr = MemoryAlloc(Total);
	if (Ptr) {
		Byte *B = (Byte *)Ptr;
		for (Size i = 0; i < Total; i++)
			B[i] = 0;
	}
	return Ptr;
}

void MemoryFree(void *Pointer)
{
	if (Pointer == 0)
		return;

	MemBlock *Block = (MemBlock *)Pointer - 1;
	HeapRegion *Reg = FindRegion(Block);
	if (!Reg)
		return;

	SET_FREE(Block);
	Size TotalSize = BLOCK_SIZE(Block);

	MemBlock *Next = NEXT_BLOCK(Block);
	if ((Byte *)Next < (Byte *)Reg->End && IS_FREE(Next)) {
		FreeListRemove((FreeBlock *)(Next + 1));
		TotalSize += BLOCK_SIZE(Next);
	}

	if ((Byte *)Block > (Byte *)Reg + sizeof(HeapRegion)) {
		BlockFooter *PrevFooter =
			(BlockFooter *)((Byte *)Block - sizeof(BlockFooter));
		Size PrevSizeField = PrevFooter->Size;
		if (SIZE_IS_FREE(PrevSizeField)) {
			Size PrevBlockSize = SIZE_BLOCK_SZ(PrevSizeField);
			MemBlock *Prev = (MemBlock *)((Byte *)Block - PrevBlockSize);
			FreeListRemove((FreeBlock *)(Prev + 1));
			Block = Prev;
			TotalSize += PrevBlockSize;
		}
	}

	Block->Size = TotalSize | FREE_FLAG;
	Block->Pad = 0;
	FOOTER(Block)->Size = Block->Size;

	FreeListInsert((FreeBlock *)(Block + 1));
}

void *Map(Size Length, U32 Flags)
{
	return (void *)Syscall2(SyscallMap, Length, Flags);
}

void Unmap(void *Address, Size Length)
{
	Syscall2(SyscallUnmap, (U64)Address, Length);
}

U64 SharedMemCreate(Size Size)
{
	return Syscall1(SyscallSharedMemCreate, (U64)Size);
}

void *SharedMemMap(U64 Id)
{
	return (void *)Syscall1(SyscallSharedMemMap, Id);
}

void SharedMemUnmap(void *Address)
{
	Syscall1(SyscallSharedMemUnmap, (U64)Address);
}

void SharedMemDestroy(U64 Id)
{
	Syscall1(SyscallSharedMemDestroy, Id);
}

void *malloc(Size Bytes)
{
	return MemoryAlloc(Bytes);
}
void free(void *Pointer)
{
	MemoryFree(Pointer);
}
void *realloc(void *Pointer, Size Bytes)
{
	return MemoryRealloc(Pointer, Bytes);
}
void *calloc(Size Num, Size Size)
{
	return MemoryCalloc(Num, Size);
}
void *zalloc(Size Bytes)
{
	return MemoryCalloc(1, Bytes);
}