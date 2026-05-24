#include <Filesystem/Fat32.h>

#include <Device/Disk.h>
#include <Library/DebugLog.h>
#include <Memory/Allocator.h>

#define MbrPartitionTable 446u
#define MbrPartitionSize 16u
#define MbrPartitionCount 4u

#define Fat32DirectoryEntrySize 32u
#define Fat32EndOfChain 0x0FFFFFF8u
#define Fat32ClusterMask 0x0FFFFFFFu
#define Fat32LongNameMax 128u
#define Fat32LongNameLast 0x40u

#define Fat32AttrVolumeId 0x08
#define Fat32AttrLongName 0x0F

#define NameCasingLowerBase 0x08
#define NameCasingLowerExt 0x10

static uint16_t ReadLe16(const uint8_t *Data)
{
	return (uint16_t)Data[0] | ((uint16_t)Data[1] << 8);
}

static uint32_t ReadLe32(const uint8_t *Data)
{
	return (uint32_t)Data[0] | ((uint32_t)Data[1] << 8) |
		   ((uint32_t)Data[2] << 16) | ((uint32_t)Data[3] << 24);
}

static char AsciiUpper(char Character)
{
	if (Character >= 'a' && Character <= 'z') {
		return (char)(Character - ('a' - 'A'));
	}

	return Character;
}

static char AsciiLower(char Character)
{
	if (Character >= 'A' && Character <= 'Z') {
		return (char)(Character + ('a' - 'A'));
	}

	return Character;
}

static bool IsFat32Partition(uint8_t Type)
{
	return Type == 0x0B || Type == 0x0C;
}

static bool FindPartition(const BlockFs *Block, uint8_t *SectorBuffer,
						  uint32_t *PartitionLba)
{
	if (!BlockFsReadSectors(Block, 0, 1, SectorBuffer)) {
		BootError("FAT32", "failed to read MBR");
		return false;
	}

	for (uint8_t Index = 0; Index < MbrPartitionCount; ++Index) {
		const uint8_t *Entry =
			SectorBuffer + MbrPartitionTable + (Index * MbrPartitionSize);
		uint8_t Type = Entry[4];

		if (IsFat32Partition(Type)) {
			*PartitionLba = ReadLe32(Entry + 8);
			DebugLog("FAT32", "partition %u type 0x%x at LBA %u",
					 (unsigned int)Index, (unsigned int)Type,
					 (unsigned int)*PartitionLba);
			return true;
		}
	}

	DebugLog("FAT32", "no FAT32 partition found");
	return false;
}

static uint32_t ClusterToLba(const Fat32Volume *Volume, uint32_t Cluster)
{
	return Volume->DataLba +
		   ((Cluster - 2u) * (uint32_t)Volume->SectorsPerCluster);
}

static bool ReadFatEntry(const Fat32Volume *Volume, uint32_t Cluster,
						 uint32_t *NextCluster)
{
	uint32_t Offset = Cluster * 4u;
	uint32_t Sector = Volume->FatLba + (Offset / DiskSectorSize);
	uint32_t SectorOffset = Offset % DiskSectorSize;

	if (!BlockFsReadSectors(&Volume->Block, Sector, 1, Volume->SectorBuffer)) {
		BootError("FAT32", "failed to read FAT sector %u", (unsigned int)Sector);
		return false;
	}

	*NextCluster =
		ReadLe32(Volume->SectorBuffer + SectorOffset) & Fat32ClusterMask;
	return true;
}

static bool MakeShortNameN(const char *Name, size_t NameLength,
						   char ShortName[11])
{
	uint8_t Index = 0;
	size_t Position = 0;

	if (Name == 0 || NameLength == 0) {
		return false;
	}

	for (uint8_t Fill = 0; Fill < 11; ++Fill) {
		ShortName[Fill] = ' ';
	}

	while (Position < NameLength && Name[Position] != '.') {
		if (Index >= 8) {
			return false;
		}

		ShortName[Index++] = AsciiUpper(Name[Position++]);
	}

	if (Position < NameLength && Name[Position] == '.') {
		++Position;
		Index = 8;

		while (Position < NameLength) {
			if (Name[Position] == '.' || Index >= 11) {
				return false;
			}

			ShortName[Index++] = AsciiUpper(Name[Position++]);
		}
	}

	return true;
}

static bool ShortNamesEqual(const uint8_t *EntryName, const char ShortName[11])
{
	for (uint8_t Index = 0; Index < 11; ++Index) {
		if ((char)EntryName[Index] != ShortName[Index]) {
			return false;
		}
	}

	return true;
}

static bool NameEqualsN(const char *Left, const char *Right, size_t RightLength)
{
	size_t Index = 0;

	for (; Index < RightLength; ++Index) {
		if (AsciiLower(Left[Index]) != AsciiLower(Right[Index])) {
			return false;
		}
	}

	return Left[Index] == '\0';
}

static char ApplyNameCasing(char Character, uint8_t NameCasing)
{
	if ((NameCasing & NameCasingLowerBase) != 0) {
		return AsciiLower(Character);
	}

	return Character;
}

static char ApplyExtensionCasing(char Character, uint8_t NameCasing)
{
	if ((NameCasing & NameCasingLowerExt) != 0) {
		return AsciiLower(Character);
	}

	return Character;
}

static void FormatShortName(const uint8_t *Entry, char Out[13])
{
	uint8_t OutIndex = 0;
	uint8_t BaseEnd = 8;
	uint8_t ExtEnd = 11;
	uint8_t NameCasing = Entry[12];

	while (BaseEnd > 0 && Entry[BaseEnd - 1] == ' ') {
		--BaseEnd;
	}

	while (ExtEnd > 8 && Entry[ExtEnd - 1] == ' ') {
		--ExtEnd;
	}

	for (uint8_t Index = 0; Index < BaseEnd; ++Index) {
		Out[OutIndex++] = ApplyNameCasing((char)Entry[Index], NameCasing);
	}

	if (ExtEnd > 8) {
		Out[OutIndex++] = '.';

		for (uint8_t Index = 8; Index < ExtEnd; ++Index) {
			Out[OutIndex++] =
				ApplyExtensionCasing((char)Entry[Index], NameCasing);
		}
	}

	Out[OutIndex] = '\0';
}

static void FillEntry(const uint8_t *RawEntry, Fat32DirectoryEntry *Entry)
{
	FormatShortName(RawEntry, Entry->Name);
	Entry->Attributes = RawEntry[11];
	Entry->FirstCluster =
		((uint32_t)ReadLe16(RawEntry + 20) << 16) | ReadLe16(RawEntry + 26);
	Entry->Size = ReadLe32(RawEntry + 28);
}

static bool ShouldSkipEntry(const uint8_t *Entry)
{
	uint8_t First = Entry[0];
	uint8_t Attributes = Entry[11];

	return First == 0xE5 || (Attributes & Fat32AttrVolumeId) != 0;
}

static void ClearLongName(char LongName[Fat32LongNameMax],
						  bool *HaveLongNameOut)
{
	LongName[0] = '\0';
	*HaveLongNameOut = false;
}

static void StoreLongNameChar(char LongName[Fat32LongNameMax], size_t Index,
							  uint16_t Character)
{
	if (Index + 1u >= Fat32LongNameMax || Character == 0x0000 ||
		Character == 0xffff) {
		return;
	}

	LongName[Index] = Character < 0x80 ? (char)Character : '?';
	LongName[Index + 1u] = '\0';
}

static void ReadLongNameEntry(const uint8_t *Entry,
							  char LongName[Fat32LongNameMax],
							  bool *HaveLongNameOut)
{
	static const uint8_t Offsets[13] = {
		1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30,
	};
	uint8_t Sequence = Entry[0] & 0x1F;

	if ((Entry[0] & Fat32LongNameLast) != 0) {
		LongName[0] = '\0';
	}

	if (Sequence == 0) {
		ClearLongName(LongName, HaveLongNameOut);
		return;
	}

	size_t BaseIndex = ((size_t)Sequence - 1u) * 13u;
	for (size_t Index = 0; Index < 13u; ++Index) {
		uint16_t Character = ReadLe16(Entry + Offsets[Index]);
		StoreLongNameChar(LongName, BaseIndex + Index, Character);
	}

	*HaveLongNameOut = true;
}

static bool FindDirectoryEntry(const Fat32Volume *Volume,
							   uint32_t DirectoryCluster,
							   const char ShortName[11],
							   const char *SearchLongName,
							   size_t SearchLongNameLength,
							   Fat32DirectoryEntry *Found)
{
	uint32_t Cluster = DirectoryCluster;
	char LongNameBuffer[Fat32LongNameMax];
	bool HaveLongName = false;

	ClearLongName(LongNameBuffer, &HaveLongName);

	while (Cluster < Fat32EndOfChain) {
		uint32_t ClusterLba = ClusterToLba(Volume, Cluster);

		for (uint8_t Sector = 0; Sector < Volume->SectorsPerCluster; ++Sector) {
			if (!BlockFsReadSectors(&Volume->Block, ClusterLba + Sector, 1,
									Volume->SectorBuffer)) {
				BootError("FAT32", "failed to read directory cluster %u",
						 (unsigned int)Cluster);
				return false;
			}

			for (uint16_t Offset = 0; Offset < DiskSectorSize;
				 Offset += Fat32DirectoryEntrySize) {
				const uint8_t *RawEntry = Volume->SectorBuffer + Offset;

				if (RawEntry[0] == 0x00) {
					return true;
				}

				if (RawEntry[11] == Fat32AttrLongName) {
					ReadLongNameEntry(RawEntry, LongNameBuffer, &HaveLongName);
					continue;
				}

				if (ShouldSkipEntry(RawEntry)) {
					ClearLongName(LongNameBuffer, &HaveLongName);
					continue;
				}

				Fat32DirectoryEntry Entry;
				FillEntry(RawEntry, &Entry);

				if ((ShortName != 0 && ShortNamesEqual(RawEntry, ShortName)) ||
					(SearchLongName != 0 && HaveLongName &&
					 NameEqualsN(LongNameBuffer, SearchLongName,
								 SearchLongNameLength))) {
					*Found = Entry;
					return true;
				}

				ClearLongName(LongNameBuffer, &HaveLongName);
			}
		}

		if (!ReadFatEntry(Volume, Cluster, &Cluster)) {
			BootError("FAT32", "failed to follow root directory chain");
			return false;
		}
	}

	return true;
}

static bool FindInDirectory(const Fat32Volume *Volume,
							uint32_t DirectoryCluster, const char *Name,
							size_t NameLength, Fat32DirectoryEntry *Entry)
{
	char ShortName[11];
	bool HaveShortName = MakeShortNameN(Name, NameLength, ShortName);

	if (!HaveShortName && NameLength >= Fat32LongNameMax) {
		DebugLog("FAT32", "path segment too long");
		return false;
	}

	Entry->Name[0] = '\0';

	if (!FindDirectoryEntry(Volume, DirectoryCluster,
							HaveShortName ? ShortName : 0, Name, NameLength,
							Entry)) {
		return false;
	}

	return Entry->Name[0] != '\0';
}

static bool Fat32FindPath(const Fat32Volume *Volume, const char *Path,
						  Fat32DirectoryEntry *Entry)
{
	uint32_t DirectoryCluster = Volume->RootCluster;
	const char *Segment = Path;

	while (Segment != 0 && *Segment == '/') {
		++Segment;
	}

	if (Segment == 0 || *Segment == '\0') {
		Entry->Name[0] = '\0';
		Entry->Attributes = Fat32AttrDirectory;
		Entry->FirstCluster = Volume->RootCluster;
		Entry->Size = 0;
		return true;
	}

	for (;;) {
		const char *Cursor = Segment;

		while (*Cursor != '\0' && *Cursor != '/') {
			++Cursor;
		}

		size_t SegmentLength = (size_t)(Cursor - Segment);
		if (!FindInDirectory(Volume, DirectoryCluster, Segment, SegmentLength,
							 Entry)) {
			return false;
		}

		while (*Cursor == '/') {
			++Cursor;
		}

		if (*Cursor == '\0') {
			return true;
		}

		if ((Entry->Attributes & Fat32AttrDirectory) == 0 ||
			Entry->FirstCluster < 2) {
			return false;
		}

		DirectoryCluster = Entry->FirstCluster;
		Segment = Cursor;
	}
}

bool Fat32Mount(Fat32Volume *Volume, const BlockDevice *Device)
{
	uint32_t PartitionLba;

	if (Volume == 0 || Device == 0) {
		BootError("FAT32", "mount rejected: null volume");
		return false;
	}

	Volume->Block.Device = Device;
	Volume->SectorBuffer = Alloc(DiskSectorSize, DiskSectorSize);
	Volume->ScratchBuffer = Alloc(DiskSectorSize, DiskSectorSize);

	if (Volume->SectorBuffer == 0 || Volume->ScratchBuffer == 0) {
		BootError("FAT32", "failed to allocate sector buffers");
		return false;
	}

	if (!FindPartition(&Volume->Block, Volume->SectorBuffer, &PartitionLba)) {
		return false;
	}

	if (!BlockFsReadSectors(&Volume->Block, PartitionLba, 1,
							Volume->SectorBuffer)) {
		BootError("FAT32", "failed to read boot sector at LBA %u",
				 (unsigned int)PartitionLba);
		return false;
	}

	uint16_t BytesPerSector = ReadLe16(Volume->SectorBuffer + 11);
	uint16_t ReservedSectors = ReadLe16(Volume->SectorBuffer + 14);
	uint8_t FatCount = Volume->SectorBuffer[16];
	uint32_t SectorsPerFat = ReadLe32(Volume->SectorBuffer + 36);
	uint32_t RootCluster = ReadLe32(Volume->SectorBuffer + 44);
	uint8_t SectorsPerCluster = Volume->SectorBuffer[13];

	if (BytesPerSector != DiskSectorSize || SectorsPerCluster == 0 ||
		FatCount == 0 || SectorsPerFat == 0 || RootCluster < 2) {
		BootError("FAT32",
				 "unsupported BPB: bytes/sector %u SPC %u FATs %u SPF %u "
				 "root %u",
				 (unsigned int)BytesPerSector, (unsigned int)SectorsPerCluster,
				 (unsigned int)FatCount, (unsigned int)SectorsPerFat,
				 (unsigned int)RootCluster);
		return false;
	}

	Volume->PartitionLba = PartitionLba;
	Volume->SectorsPerCluster = SectorsPerCluster;
	Volume->FatCount = FatCount;
	Volume->SectorsPerFat = SectorsPerFat;
	Volume->FatLba = PartitionLba + ReservedSectors;
	Volume->DataLba = Volume->FatLba + (FatCount * SectorsPerFat);
	Volume->RootCluster = RootCluster;

	DebugLog("FAT32", "BPB SPC %u FATs %u SPF %u data LBA %u",
			(unsigned int)Volume->SectorsPerCluster,
			(unsigned int)Volume->FatCount, (unsigned int)Volume->SectorsPerFat,
			(unsigned int)Volume->DataLba);
	return true;
}

size_t Fat32ReadFile(const Fat32Volume *Volume,
					 const Fat32DirectoryEntry *Entry, void *Buffer,
					 size_t Length)
{
	return Fat32ReadFileAt(Volume, Entry, 0, Buffer, Length);
}

size_t Fat32ReadFileAt(const Fat32Volume *Volume,
					   const Fat32DirectoryEntry *Entry, size_t Offset,
					   void *Buffer, size_t Length)
{
	if (Volume == 0 || Entry == 0 || Buffer == 0) {
		BootError("FAT32", "read rejected: null argument");
		return 0;
	}

	if (Offset >= Entry->Size || Length == 0) {
		return 0;
	}

	uint8_t *Out = Buffer;
	size_t ClusterSize = (size_t)Volume->SectorsPerCluster * DiskSectorSize;
	size_t Remaining =
		(Entry->Size - Offset) < Length ? (Entry->Size - Offset) : Length;
	size_t TotalRead = 0;
	uint32_t Cluster = Entry->FirstCluster;

	while (Offset >= ClusterSize && Cluster < Fat32EndOfChain) {
		if (!ReadFatEntry(Volume, Cluster, &Cluster)) {
			BootError("FAT32", "failed to seek within '%s'", Entry->Name);
			return 0;
		}

		Offset -= ClusterSize;
	}

	while (Remaining > 0 && Cluster < Fat32EndOfChain) {
		uint32_t ClusterLba = ClusterToLba(Volume, Cluster);

		for (uint8_t Sector = 0;
			 Sector < Volume->SectorsPerCluster && Remaining > 0; ++Sector) {
			if (Offset >= DiskSectorSize) {
				Offset -= DiskSectorSize;
				continue;
			}

			if (!BlockFsReadSectors(&Volume->Block, ClusterLba + Sector, 1,
									Volume->ScratchBuffer)) {
				BootError("FAT32", "short read from '%s' at cluster %u",
						 Entry->Name, (unsigned int)Cluster);
				return TotalRead;
			}

			size_t SectorOffset = Offset;
			size_t Available = DiskSectorSize - SectorOffset;
			size_t Chunk = Remaining < Available ? Remaining : Available;

			for (size_t Index = 0; Index < Chunk; ++Index) {
				Out[TotalRead + Index] =
					Volume->ScratchBuffer[SectorOffset + Index];
			}

			TotalRead += Chunk;
			Remaining -= Chunk;
			Offset = 0;
		}

		if (Remaining > 0 && !ReadFatEntry(Volume, Cluster, &Cluster)) {
			BootError("FAT32", "failed to continue reading '%s'", Entry->Name);
			return TotalRead;
		}
	}

	return TotalRead;
}

static bool Fat32VfsOpen(void *Filesystem, const char *Path, VfsNode *Node)
{
	if (Filesystem == 0 || Node == 0) {
		return false;
	}

	Fat32DirectoryEntry *Entry = (Fat32DirectoryEntry *)Node->Storage;

	if (!Fat32FindPath((const Fat32Volume *)Filesystem, Path, Entry)) {
		return false;
	}

	Node->Size = Entry->Size;
	Node->Directory = (Entry->Attributes & Fat32AttrDirectory) != 0;
	return true;
}

static size_t Fat32VfsRead(void *Filesystem, const VfsNode *Node, size_t Offset,
						   void *Buffer, size_t Length)
{
	if (Filesystem == 0 || Node == 0) {
		return 0;
	}

	const Fat32DirectoryEntry *Entry =
		(const Fat32DirectoryEntry *)Node->Storage;
	return Fat32ReadFileAt((const Fat32Volume *)Filesystem, Entry, Offset,
						   Buffer, Length);
}

static const VfsFilesystemOps Fat32FilesystemOps = {
	.Open = Fat32VfsOpen,
	.Read = Fat32VfsRead,
};

const VfsFilesystemOps *Fat32GetFilesystemOps(void)
{
	return &Fat32FilesystemOps;
}
