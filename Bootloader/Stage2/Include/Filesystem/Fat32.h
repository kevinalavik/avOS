#ifndef FS_FAT32_H
#define FS_FAT32_H

#include <Filesystem/BlockFs.h>
#include <Filesystem/Vfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define Fat32AttrDirectory 0x10u

typedef struct Fat32Volume {
	BlockFs Block;
	uint8_t *SectorBuffer;
	uint8_t *ScratchBuffer;
	uint32_t PartitionLba;
	uint32_t FatLba;
	uint32_t DataLba;
	uint32_t RootCluster;
	uint32_t SectorsPerFat;
	uint8_t SectorsPerCluster;
	uint8_t FatCount;
} Fat32Volume;

typedef struct Fat32DirectoryEntry {
	char Name[13];
	uint8_t Attributes;
	uint32_t FirstCluster;
	uint32_t Size;
} Fat32DirectoryEntry;

bool Fat32Mount(Fat32Volume *Volume, const BlockDevice *Device);
size_t Fat32ReadFile(const Fat32Volume *Volume,
					 const Fat32DirectoryEntry *Entry, void *Buffer,
					 size_t Length);
size_t Fat32ReadFileAt(const Fat32Volume *Volume,
					   const Fat32DirectoryEntry *Entry, size_t Offset,
					   void *Buffer, size_t Length);
const VfsFilesystemOps *Fat32GetFilesystemOps(void);

#endif
