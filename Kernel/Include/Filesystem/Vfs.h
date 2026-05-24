#ifndef FS_VFS_H
#define FS_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VfsPathMax 256u

enum {
	VfsNodeFlagDirectory = 1u << 0,
};

typedef struct VfsStat {
	uint32_t Flags;
	size_t Size;
} VfsStat;

typedef struct VfsNode {
	uint32_t Flags;
	void *PrivateData;
} VfsNode;

typedef struct VfsDirent {
	char Name[128];
	VfsStat Stat;
} VfsDirent;

typedef struct VfsFilesystemOps {
	bool (*Open)(void *Filesystem, const char *Path, VfsNode *Node);
	void (*Close)(void *Filesystem, VfsNode *Node);
	bool (*Stat)(void *Filesystem, const VfsNode *Node, VfsStat *Out);
	size_t (*Read)(void *Filesystem, const VfsNode *Node, size_t Offset,
				   void *Buffer, size_t Length);
	bool (*ReadDir)(void *Filesystem, const VfsNode *Directory, size_t Index,
				 VfsDirent *Out);
} VfsFilesystemOps;

typedef struct VfsVolumeInfo {
	char Id;
	const void *Filesystem;
	const VfsFilesystemOps *Ops;
} VfsVolumeInfo;

typedef struct File {
	const VfsFilesystemOps *Ops;
	void *Filesystem;
	VfsNode Node;
	VfsStat Stat;
	uint32_t Magic;
	size_t Position;
	bool Open;
} File;

typedef struct Directory {
	const VfsFilesystemOps *Ops;
	void *Filesystem;
	VfsNode Node;
	VfsStat Stat;
	uint32_t Magic;
	size_t Index;
	bool Open;
} Directory;

bool VfsMount(char VolumeId, const VfsFilesystemOps *Ops,
			  void *Filesystem);
size_t VfsGetVolumeCount(void);
bool VfsGetVolumeInfo(size_t Index, VfsVolumeInfo *Out);

bool FileOpen(const char *Path, File *Handle);
size_t FileRead(File *Handle, void *Buffer, size_t Length);
bool FileSeek(File *Handle, size_t Position);
size_t FileTell(const File *Handle);
size_t FileSize(const File *Handle);
bool FileStat(const File *Handle, VfsStat *Out);
bool FileIsOpen(const File *Handle);
void FileClose(File *Handle);

bool DirOpen(const char *Path, Directory *Handle);
bool DirRead(Directory *Handle, VfsDirent *Out);
bool DirStat(const Directory *Handle, VfsStat *Out);
bool DirIsOpen(const Directory *Handle);
void DirClose(Directory *Handle);

bool VfsStatPath(const char *Path, VfsStat *Out);

#endif
