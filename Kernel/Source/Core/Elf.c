#include <Core/Elf.h>

#include <Core/Log.h>
#include <Core/Scheduler.h>
#include <Filesystem/Vfs.h>
#include <Memory/Heap.h>
#include <Memory/Paging.h>
#include <Memory/PageDb.h>
#include <Memory/Vmm.h>

#include <stddef.h>
#include <stdint.h>

#define ElfMagic0 0x7f
#define ElfMagic1 'E'
#define ElfMagic2 'L'
#define ElfMagic3 'F'
#define ElfClass64 2
#define ElfDataLittle 1
#define ElfTypeDyn 3
#define ElfMachineX86_64 62
#define ElfVersionCurrent 1
#define ElfProgramTypeLoad 1
#define ElfProgramTypeDynamic 2
#define ElfDynamicNull 0
#define ElfDynamicRela 7
#define ElfDynamicRelaSize 8
#define ElfDynamicRelaEnt 9
#define ElfRelocX86_64None 0
#define ElfRelocX86_64Relative 8
#define ElfRelocType(Info) ((uint32_t)(Info))
#define ElfRelocSymbol(Info) ((uint32_t)((Info) >> 32))
#define UserImageBaseStart 0x0000000040000000ull
#define UserImageStride 0x0000000010000000ull
#define UserStackBaseStart 0x0000007000000000ull
#define UserStackStride 0x0000000000200000ull
#define UserDefaultStackSize (16ull * PageSize)

typedef struct Elf64Header {
	uint8_t Ident[16];
	uint16_t Type;
	uint16_t Machine;
	uint32_t Version;
	uint64_t Entry;
	uint64_t ProgramHeaderOffset;
	uint64_t SectionHeaderOffset;
	uint32_t Flags;
	uint16_t HeaderSize;
	uint16_t ProgramHeaderEntrySize;
	uint16_t ProgramHeaderCount;
	uint16_t SectionHeaderEntrySize;
	uint16_t SectionHeaderCount;
	uint16_t SectionHeaderNameIndex;
} __attribute__((packed)) Elf64Header;

typedef struct Elf64ProgramHeader {
	uint32_t Type;
	uint32_t Flags;
	uint64_t Offset;
	uint64_t VirtualAddress;
	uint64_t PhysicalAddress;
	uint64_t FileSize;
	uint64_t MemorySize;
	uint64_t Align;
} __attribute__((packed)) Elf64ProgramHeader;

typedef struct Elf64Dynamic {
	int64_t Tag;
	uint64_t Value;
} __attribute__((packed)) Elf64Dynamic;

typedef struct Elf64Rela {
	uint64_t Offset;
	uint64_t Info;
	int64_t Addend;
} __attribute__((packed)) Elf64Rela;

typedef struct ElfImage {
	uint64_t Base;
	uint64_t Size;
	void (*Entry)(void);
	char Path[VfsPathMax];
} ElfImage;

static uint64_t NextUserImageBase = UserImageBaseStart;
static uint64_t NextUserStackBase = UserStackBaseStart;

static uint64_t AlignDown(uint64_t Value, uint64_t Alignment)
{
	return Value & ~(Alignment - 1ull);
}

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

static void MemoryCopy(void *Destination, const void *Source, size_t Size)
{
	uint8_t *Dst = (uint8_t *)Destination;
	const uint8_t *Src = (const uint8_t *)Source;

	for (size_t Index = 0; Index < Size; ++Index) {
		Dst[Index] = Src[Index];
	}
}

static void MemoryZero(void *Destination, size_t Size)
{
	uint8_t *Dst = (uint8_t *)Destination;

	for (size_t Index = 0; Index < Size; ++Index) {
		Dst[Index] = 0;
	}
}

static bool StringCopy(char *Destination, size_t DestinationSize,
					   const char *Source)
{
	size_t Index = 0;

	if (Destination == 0 || DestinationSize == 0 || Source == 0) {
		return false;
	}

	while (Source[Index] != '\0') {
		if (Index + 1 >= DestinationSize) {
			Destination[0] = '\0';
			return false;
		}

		Destination[Index] = Source[Index];
		++Index;
	}

	Destination[Index] = '\0';
	return true;
}

static const char *PathBasename(const char *Path)
{
	const char *Base = Path;

	if (Path == 0) {
		return "";
	}

	for (const char *Cursor = Path; *Cursor != '\0'; ++Cursor) {
		if (*Cursor == '/' && Cursor[1] != '\0') {
			Base = Cursor + 1;
		}
	}

	return Base;
}

static void ElfProcessCleanup(Process *Proc)
{
	ElfImage *Image;

	if (Proc == 0 || Proc->Argument == 0) {
		return;
	}

	Image = (ElfImage *)Proc->Argument;
	if (Image->Base != 0 && Image->Size != 0) {
		VmmFreeRegion(Image->Base);
	}

	if (Proc->UserStackBase != 0 && Proc->UserStackSize != 0) {
		VmmFreeRegion(Proc->UserStackBase);
		Proc->UserStackBase = 0;
		Proc->UserStackSize = 0;
	}

	KernelFree(Image);
	Proc->Argument = 0;
}

static uint64_t ReserveUserRegion(uint64_t Size, uint64_t *Cursor,
								  uint64_t Stride)
{
	Size = AlignUp(Size, PageSize);
	if (Size == 0 || Cursor == 0 || Stride == 0) {
		return 0;
	}

	for (uint32_t Attempt = 0; Attempt < 128; ++Attempt) {
		uint64_t Base = *Cursor;
		uint64_t Advance = AlignUp(Size, Stride);
		*Cursor = Base + Advance;

		if (VmmReserveFixedRegion(
				Base, Size, VmmRegionAnonymous | VmmRegionWritable) != 0) {
			return Base;
		}
	}

	return 0;
}

static bool ElfValidateHeader(const Elf64Header *Header, size_t FileSize)
{
	if (Header == 0 || FileSize < sizeof(*Header)) {
		return false;
	}

	if (Header->Ident[0] != ElfMagic0 || Header->Ident[1] != ElfMagic1 ||
		Header->Ident[2] != ElfMagic2 || Header->Ident[3] != ElfMagic3) {
		return false;
	}

	if (Header->Ident[4] != ElfClass64 || Header->Ident[5] != ElfDataLittle ||
		Header->Version != ElfVersionCurrent ||
		Header->Machine != ElfMachineX86_64 || Header->Type != ElfTypeDyn ||
		Header->ProgramHeaderEntrySize != sizeof(Elf64ProgramHeader) ||
		Header->ProgramHeaderCount == 0) {
		return false;
	}

	if (Header->ProgramHeaderOffset > FileSize) {
		return false;
	}

	if ((uint64_t)Header->ProgramHeaderCount * sizeof(Elf64ProgramHeader) >
		FileSize - Header->ProgramHeaderOffset) {
		return false;
	}

	return true;
}

static bool ElfApplyRelocations(uint64_t LoadBase,
								const Elf64ProgramHeader *ProgramHeaders,
								uint16_t ProgramHeaderCount)
{
	const Elf64ProgramHeader *DynamicProgram = 0;
	uint64_t RelaAddress = 0;
	uint64_t RelaSize = 0;
	uint64_t RelaEntrySize = sizeof(Elf64Rela);

	for (uint16_t Index = 0; Index < ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *Program = &ProgramHeaders[Index];

		if (Program->Type == ElfProgramTypeDynamic) {
			DynamicProgram = Program;
			break;
		}
	}

	if (DynamicProgram == 0) {
		return true;
	}

	Elf64Dynamic *Dynamic =
		(Elf64Dynamic *)(uintptr_t)(LoadBase + DynamicProgram->VirtualAddress);
	uint64_t DynamicCount = DynamicProgram->MemorySize / sizeof(Elf64Dynamic);

	for (uint64_t Index = 0; Index < DynamicCount; ++Index) {
		if (Dynamic[Index].Tag == ElfDynamicNull) {
			break;
		}

		switch (Dynamic[Index].Tag) {
		case ElfDynamicRela:
			RelaAddress = Dynamic[Index].Value;
			break;
		case ElfDynamicRelaSize:
			RelaSize = Dynamic[Index].Value;
			break;
		case ElfDynamicRelaEnt:
			RelaEntrySize = Dynamic[Index].Value;
			break;
		default:
			break;
		}
	}

	if (RelaAddress == 0 || RelaSize == 0) {
		return true;
	}

	if (RelaEntrySize != sizeof(Elf64Rela)) {
		return false;
	}

	Elf64Rela *Rela = (Elf64Rela *)(uintptr_t)(LoadBase + RelaAddress);
	uint64_t RelaCount = RelaSize / RelaEntrySize;

	for (uint64_t Index = 0; Index < RelaCount; ++Index) {
		uint32_t Type = ElfRelocType(Rela[Index].Info);
		uint32_t Symbol = ElfRelocSymbol(Rela[Index].Info);
		uint64_t *Where =
			(uint64_t *)(uintptr_t)(LoadBase + Rela[Index].Offset);

		switch (Type) {
		case ElfRelocX86_64None:
			break;
		case ElfRelocX86_64Relative:
			if (Symbol != 0) {
				return false;
			}
			*Where = LoadBase + (uint64_t)Rela[Index].Addend;
			break;
		default:
			return false;
		}
	}

	return true;
}

Process *ElfSpawn(const char *Path)
{
	File Handle;
	size_t ElfFileSize;
	uint8_t *FileImage = 0;
	ElfImage *Image = 0;
	Process *Proc = 0;
	uint64_t MinAddress = UINT64_MAX;
	uint64_t MaxAddress = 0;
	uint64_t LoadBase;
	uint64_t EntryAddress;
	uint64_t UserStackBase;
	uint64_t UserStackTop;

	if (Path == 0) {
		return 0;
	}

	if (!FileOpen(Path, &Handle)) {
		LogWarn("core.elf", "unable to open '%s'", Path);
		return 0;
	}

	ElfFileSize = FileSize(&Handle);
	if (ElfFileSize < sizeof(Elf64Header)) {
		LogWarn("core.elf", "file too small for ELF '%s'", Path);
		FileClose(&Handle);
		return 0;
	}

	FileImage = KernelAlloc(ElfFileSize);
	if (FileImage == 0) {
		FileClose(&Handle);
		return 0;
	}

	if (FileRead(&Handle, FileImage, ElfFileSize) != ElfFileSize) {
		LogWarn("core.elf", "failed to read ELF '%s'", Path);
		FileClose(&Handle);
		KernelFree(FileImage);
		return 0;
	}

	FileClose(&Handle);

	const Elf64Header *Header = (const Elf64Header *)FileImage;
	if (!ElfValidateHeader(Header, ElfFileSize)) {
		LogWarn("core.elf", "unsupported ELF '%s'", Path);
		KernelFree(FileImage);
		return 0;
	}

	const Elf64ProgramHeader *ProgramHeaders =
		(const Elf64ProgramHeader *)(FileImage + Header->ProgramHeaderOffset);

	for (uint16_t Index = 0; Index < Header->ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *Program = &ProgramHeaders[Index];
		if (Program->Type != ElfProgramTypeLoad) {
			continue;
		}

		if (Program->FileSize > Program->MemorySize ||
			Program->Offset > ElfFileSize ||
			Program->FileSize > ElfFileSize - Program->Offset) {
			LogWarn("core.elf", "invalid load segment in '%s'", Path);
			KernelFree(FileImage);
			return 0;
		}

		if (Program->VirtualAddress < MinAddress) {
			MinAddress = Program->VirtualAddress;
		}

		uint64_t SegmentEnd = Program->VirtualAddress + Program->MemorySize;
		if (SegmentEnd > MaxAddress) {
			MaxAddress = SegmentEnd;
		}
	}

	if (MinAddress == UINT64_MAX || MaxAddress <= MinAddress) {
		LogWarn("core.elf", "no loadable segments in '%s'", Path);
		KernelFree(FileImage);
		return 0;
	}

	uint64_t ImageSize =
		AlignUp(MaxAddress - AlignDown(MinAddress, PageSize), PageSize);
	uint64_t ImageBase =
		ReserveUserRegion(ImageSize, &NextUserImageBase, UserImageStride);
	if (ImageBase == 0) {
		LogError("core.elf", "failed to reserve image space for '%s'", Path);
		KernelFree(FileImage);
		return 0;
	}

	LoadBase = ImageBase - AlignDown(MinAddress, PageSize);

	for (uint16_t Index = 0; Index < Header->ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *Program = &ProgramHeaders[Index];
		if (Program->Type != ElfProgramTypeLoad) {
			continue;
		}

		uint64_t Destination = LoadBase + Program->VirtualAddress;
		uint64_t CommitBase = AlignDown(Destination, PageSize);
		uint64_t CommitSize =
			AlignUp((Destination - CommitBase) + Program->MemorySize, PageSize);

		if (!VmmCommitRange(CommitBase, CommitSize,
							PagingFlagWritable | PagingFlagUser)) {
			LogError("core.elf", "failed to map segment for '%s'", Path);
			VmmFreeRegion(ImageBase);
			KernelFree(FileImage);
			return 0;
		}

		MemoryZero((void *)(uintptr_t)Destination, Program->MemorySize);
		MemoryCopy((void *)(uintptr_t)Destination, FileImage + Program->Offset,
				   Program->FileSize);
	}

	if (!ElfApplyRelocations(LoadBase, ProgramHeaders,
							 Header->ProgramHeaderCount)) {
		LogError("core.elf", "failed to relocate '%s'", Path);
		VmmFreeRegion(ImageBase);
		KernelFree(FileImage);
		return 0;
	}

	if (!ElfApplyRelocations(LoadBase, ProgramHeaders,
							 Header->ProgramHeaderCount)) {
		LogError("core.elf", "failed to relocate '%s'", Path);
		VmmFreeRegion(ImageBase);
		KernelFree(FileImage);
		return false;
	}

	EntryAddress = LoadBase + Header->Entry;

	KernelFree(FileImage);

	Image = KernelZalloc(sizeof(*Image));
	if (Image == 0) {
		VmmFreeRegion(ImageBase);
		return 0;
	}

	Image->Base = ImageBase;
	Image->Size = ImageSize;
	Image->Entry = (void (*)(void))(uintptr_t)EntryAddress;
	StringCopy(Image->Path, sizeof(Image->Path), Path);

	UserStackBase = ReserveUserRegion(UserDefaultStackSize, &NextUserStackBase,
									  UserStackStride);
	if (UserStackBase == 0 ||
		!VmmCommitRange(UserStackBase, UserDefaultStackSize,
						PagingFlagWritable | PagingFlagUser)) {
		if (UserStackBase != 0) {
			VmmFreeRegion(UserStackBase);
		}
		VmmFreeRegion(ImageBase);
		KernelFree(Image);
		return 0;
	}
	UserStackTop = UserStackBase + UserDefaultStackSize;

	Proc = SchedulerCreateUserProcess(
		PathBasename(Path), EntryAddress, UserStackBase, UserDefaultStackSize,
		UserStackTop, Image, ProcessDefaultStackPages);
	if (Proc == 0) {
		VmmFreeRegion(UserStackBase);
		VmmFreeRegion(ImageBase);
		KernelFree(Image);
		return 0;
	}

	Proc->Cleanup = ElfProcessCleanup;
	LogOk("core.elf", "loaded '%s' entry=0x%llx base=0x%llx size=0x%llx", Path,
		  (unsigned long long)(uintptr_t)Image->Entry,
		  (unsigned long long)Image->Base, (unsigned long long)Image->Size);
	return Proc;
}

bool ElfExec(const char *Path)
{
	Process *Current = SchedulerCurrent();
	File Handle;
	size_t ElfFileSize;
	uint8_t *FileImage;
	ElfImage *NewImage;
	uint64_t MinAddress = UINT64_MAX;
	uint64_t MaxAddress = 0;
	uint64_t LoadBase;
	uint64_t EntryAddress;
	uint64_t UserStackBase;
	uint64_t UserStackTop;

	if (Path == 0 || Current == 0) {
		return false;
	}

	if (!FileOpen(Path, &Handle)) {
		return false;
	}

	ElfFileSize = FileSize(&Handle);
	if (ElfFileSize < sizeof(Elf64Header)) {
		FileClose(&Handle);
		return false;
	}

	FileImage = KernelAlloc(ElfFileSize);
	if (FileImage == 0) {
		FileClose(&Handle);
		return false;
	}

	if (FileRead(&Handle, FileImage, ElfFileSize) != ElfFileSize) {
		KernelFree(FileImage);
		FileClose(&Handle);
		return false;
	}

	FileClose(&Handle);

	const Elf64Header *Header = (const Elf64Header *)FileImage;
	if (!ElfValidateHeader(Header, ElfFileSize)) {
		KernelFree(FileImage);
		return false;
	}

	const Elf64ProgramHeader *ProgramHeaders =
		(const Elf64ProgramHeader *)(FileImage + Header->ProgramHeaderOffset);

	for (uint16_t Index = 0; Index < Header->ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *Program = &ProgramHeaders[Index];
		if (Program->Type != ElfProgramTypeLoad) {
			continue;
		}

		if (Program->FileSize > Program->MemorySize ||
			Program->Offset > ElfFileSize ||
			Program->FileSize > ElfFileSize - Program->Offset) {
			KernelFree(FileImage);
			return false;
		}

		if (Program->VirtualAddress < MinAddress) {
			MinAddress = Program->VirtualAddress;
		}

		uint64_t SegmentEnd = Program->VirtualAddress + Program->MemorySize;
		if (SegmentEnd > MaxAddress) {
			MaxAddress = SegmentEnd;
		}
	}

	if (MinAddress == UINT64_MAX || MaxAddress <= MinAddress) {
		KernelFree(FileImage);
		return false;
	}

	/* Free the old process image and stack before loading the new one */
	if (Current->Argument != 0) {
		ElfImage *Old = (ElfImage *)Current->Argument;
		if (Old->Base != 0 && Old->Size != 0) {
			VmmFreeRegion(Old->Base);
		}
		KernelFree(Old);
		Current->Argument = 0;
	}

	if (Current->UserStackBase != 0 && Current->UserStackSize != 0) {
		VmmFreeRegion(Current->UserStackBase);
		Current->UserStackBase = 0;
		Current->UserStackSize = 0;
	}

	uint64_t ImageSize =
		AlignUp(MaxAddress - AlignDown(MinAddress, PageSize), PageSize);
	uint64_t ImageBase =
		ReserveUserRegion(ImageSize, &NextUserImageBase, UserImageStride);
	if (ImageBase == 0) {
		KernelFree(FileImage);
		return false;
	}

	LoadBase = ImageBase - AlignDown(MinAddress, PageSize);

	for (uint16_t Index = 0; Index < Header->ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *Program = &ProgramHeaders[Index];
		if (Program->Type != ElfProgramTypeLoad) {
			continue;
		}

		uint64_t Destination = LoadBase + Program->VirtualAddress;
		uint64_t CommitBase = AlignDown(Destination, PageSize);
		uint64_t CommitSize =
			AlignUp((Destination - CommitBase) + Program->MemorySize, PageSize);

		if (!VmmCommitRange(CommitBase, CommitSize,
							PagingFlagWritable | PagingFlagUser)) {
			VmmFreeRegion(ImageBase);
			KernelFree(FileImage);
			return false;
		}

		MemoryZero((void *)(uintptr_t)Destination, Program->MemorySize);
		MemoryCopy((void *)(uintptr_t)Destination, FileImage + Program->Offset,
				   Program->FileSize);
	}

	EntryAddress = LoadBase + Header->Entry;

	KernelFree(FileImage);

	NewImage = KernelZalloc(sizeof(*NewImage));
	if (NewImage == 0) {
		VmmFreeRegion(ImageBase);
		return false;
	}

	NewImage->Base = ImageBase;
	NewImage->Size = ImageSize;
	NewImage->Entry = (void (*)(void))(uintptr_t)EntryAddress;
	StringCopy(NewImage->Path, sizeof(NewImage->Path), Path);

	UserStackBase = ReserveUserRegion(UserDefaultStackSize, &NextUserStackBase,
									  UserStackStride);
	if (UserStackBase == 0 ||
		!VmmCommitRange(UserStackBase, UserDefaultStackSize,
						PagingFlagWritable | PagingFlagUser)) {
		if (UserStackBase != 0) {
			VmmFreeRegion(UserStackBase);
		}
		VmmFreeRegion(ImageBase);
		KernelFree(NewImage);
		return false;
	}
	UserStackTop = UserStackBase + UserDefaultStackSize;

	Current->Argument = NewImage;
	Current->UserRip = EntryAddress;
	Current->UserRsp = UserStackTop;
	Current->UserStackBase = UserStackBase;
	Current->UserStackSize = UserDefaultStackSize;
	Current->Cleanup = ElfProcessCleanup;
	StringCopy(Current->Name, ProcessNameMax, PathBasename(Path));

	LogOk("core.elf", "exec '%s' entry=0x%llx", Path,
		  (unsigned long long)EntryAddress);
	return true;
}