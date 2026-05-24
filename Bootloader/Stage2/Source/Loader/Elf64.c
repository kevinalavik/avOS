#include <Loader/Elf64.h>

#include <Filesystem/Vfs.h>
#include <Library/DebugLog.h>
#include <Memory/Allocator.h>

#define ElfMagic0 0x7fu
#define ElfMagic1 'E'
#define ElfMagic2 'L'
#define ElfMagic3 'F'
#define ElfClass64 2u
#define ElfData2Lsb 1u
#define ElfVersionCurrent 1u
#define ElfTypeExec 2u
#define ElfMachineX86_64 62u
#define ElfProgramLoad 1u
#define ElfHigherHalfBase 0xffffffff80000000ull
#define ElfHigherHalfMappedSize 0x40000000ull

static uint64_t LoadedBase;
static uint64_t LoadedEnd;

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
	uint16_t SectionHeaderStringIndex;
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

static bool ReadWholeFile(const char *Path, uint8_t **BufferOut,
						  size_t *SizeOut)
{
	File FileHandle;

	if (!FileOpen(Path, &FileHandle)) {
		return false;
	}

	size_t Size = FileSize(&FileHandle);
	uint8_t *Buffer = Alloc(Size, 16);
	if (Buffer == 0) {
		FileClose(&FileHandle);
		return false;
	}

	size_t Read = FileRead(&FileHandle, Buffer, Size);
	FileClose(&FileHandle);

	if (Read != Size) {
		return false;
	}

	*BufferOut = Buffer;
	*SizeOut = Size;
	return true;
}

static bool IsValidHeader(const Elf64Header *Header, size_t FileSize)
{
	if (FileSize < sizeof(Elf64Header)) {
		return false;
	}

	if (Header->Ident[0] != ElfMagic0 || Header->Ident[1] != ElfMagic1 ||
		Header->Ident[2] != ElfMagic2 || Header->Ident[3] != ElfMagic3) {
		return false;
	}

	return Header->Ident[4] == ElfClass64 && Header->Ident[5] == ElfData2Lsb &&
		   Header->Ident[6] == ElfVersionCurrent &&
		   Header->Type == ElfTypeExec && Header->Machine == ElfMachineX86_64 &&
		   Header->Version == ElfVersionCurrent &&
		   Header->ProgramHeaderEntrySize == sizeof(Elf64ProgramHeader);
}

static bool RangeFits(uint64_t Offset, uint64_t Length, size_t FileSize)
{
	return Offset <= FileSize && Length <= (uint64_t)FileSize - Offset;
}

static bool TranslateHigherHalfAddress(uint64_t Address,
									   uint32_t *PhysicalAddress)
{
	if (Address < ElfHigherHalfBase ||
		Address >= ElfHigherHalfBase + ElfHigherHalfMappedSize) {
		return false;
	}

	*PhysicalAddress = (uint32_t)(Address - ElfHigherHalfBase);
	return true;
}

static bool ResolveLoadPhysicalAddress(const Elf64ProgramHeader *ProgramHeader,
									   uint32_t *PhysicalAddress)
{
	if (ProgramHeader->PhysicalAddress <= UINT32_MAX) {
		*PhysicalAddress = (uint32_t)ProgramHeader->PhysicalAddress;
		return true;
	}

	if (TranslateHigherHalfAddress(ProgramHeader->PhysicalAddress,
								   PhysicalAddress)) {
		return true;
	}

	return TranslateHigherHalfAddress(ProgramHeader->VirtualAddress,
									  PhysicalAddress);
}

static bool PhysicalRangeFits(uint32_t PhysicalAddress, uint64_t Length)
{
	return Length <= UINT32_MAX &&
		   Length <= ((uint64_t)UINT32_MAX + 1u) - PhysicalAddress;
}

static void CopyMemory(uint8_t *Destination, const uint8_t *Source,
					   size_t Length)
{
	for (size_t Index = 0; Index < Length; ++Index) {
		Destination[Index] = Source[Index];
	}
}

static void ClearMemory(uint8_t *Destination, size_t Length)
{
	for (size_t Index = 0; Index < Length; ++Index) {
		Destination[Index] = 0;
	}
}

bool Elf64LoadedRange(uint64_t *BaseOut, uint64_t *EndOut)
{
	if (LoadedEnd <= LoadedBase) {
		return false;
	}

	if (BaseOut != 0) {
		*BaseOut = LoadedBase;
	}

	if (EndOut != 0) {
		*EndOut = LoadedEnd;
	}

	return true;
}

bool Elf64Load(const char *Path, uint64_t *EntryAddress)
{
	uint8_t *FileBuffer;
	size_t FileSizeBytes;
	uint64_t LoadBase = UINT64_MAX;
	uint64_t LoadEnd = 0;

	if (EntryAddress == 0 ||
		!ReadWholeFile(Path, &FileBuffer, &FileSizeBytes)) {
		return false;
	}

	const Elf64Header *Header = (const Elf64Header *)FileBuffer;
	if (!IsValidHeader(Header, FileSizeBytes)) {
		BootError("ELF64", "invalid kernel ELF '%s'", Path);
		return false;
	}

	uint64_t ProgramHeaders =
		(uint64_t)Header->ProgramHeaderCount * Header->ProgramHeaderEntrySize;
	if (!RangeFits(Header->ProgramHeaderOffset, ProgramHeaders,
				   FileSizeBytes)) {
		BootError("ELF64", "program header table out of range");
		return false;
	}

	for (uint16_t Index = 0; Index < Header->ProgramHeaderCount; ++Index) {
		const Elf64ProgramHeader *ProgramHeader =
			(const Elf64ProgramHeader *)(FileBuffer +
										 Header->ProgramHeaderOffset +
										 ((uint64_t)Index *
										  Header->ProgramHeaderEntrySize));

		if (ProgramHeader->Type != ElfProgramLoad) {
			continue;
		}

		uint32_t PhysicalAddress;
		if (ProgramHeader->FileSize > ProgramHeader->MemorySize ||
			!ResolveLoadPhysicalAddress(ProgramHeader, &PhysicalAddress) ||
			!PhysicalRangeFits(PhysicalAddress, ProgramHeader->MemorySize) ||
			!RangeFits(ProgramHeader->Offset, ProgramHeader->FileSize,
					   FileSizeBytes)) {
			BootError("ELF64", "bad load segment %u", (unsigned int)Index);
			return false;
		}

		uint8_t *Destination = (uint8_t *)PhysicalAddress;
		CopyMemory(Destination, FileBuffer + ProgramHeader->Offset,
				   (size_t)ProgramHeader->FileSize);
		ClearMemory(
			Destination + ProgramHeader->FileSize,
			(size_t)(ProgramHeader->MemorySize - ProgramHeader->FileSize));

		uint64_t SegmentEnd =
			(uint64_t)PhysicalAddress + ProgramHeader->MemorySize;
		if ((uint64_t)PhysicalAddress < LoadBase) {
			LoadBase = PhysicalAddress;
		}
		if (SegmentEnd > LoadEnd) {
			LoadEnd = SegmentEnd;
		}

		DebugLog("ELF64", "segment %u virt 0x%08x%08x phys 0x%08x mem %u",
				 (unsigned int)Index,
				 (unsigned int)(ProgramHeader->VirtualAddress >> 32),
				 (unsigned int)ProgramHeader->VirtualAddress,
				 (unsigned int)PhysicalAddress,
				 (unsigned int)ProgramHeader->MemorySize);
	}

	LoadedBase = LoadBase;
	LoadedEnd = LoadEnd;
	*EntryAddress = Header->Entry;
	DebugLog("ELF64", "entry 0x%08x%08x", (unsigned int)(Header->Entry >> 32),
		  (unsigned int)Header->Entry);
	return true;
}
