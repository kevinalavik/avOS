#ifndef FS_VFS_H
#define FS_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VfsNodeStorageSize 32u

typedef struct VfsNode {
	uintptr_t Alignment;
	uint8_t Storage[VfsNodeStorageSize];
	size_t Size;
	bool Directory;
} VfsNode;

typedef struct VfsFilesystemOps {
	bool (*Open)(void *Filesystem, const char *Path, VfsNode *Node);
	size_t (*Read)(void *Filesystem, const VfsNode *Node, size_t Offset,
				   void *Buffer, size_t Length);
} VfsFilesystemOps;

typedef struct File {
	const VfsFilesystemOps *Ops;
	void *Filesystem;
	VfsNode Node;
	size_t Position;
	bool Open;
} File;

bool VfsMountRoot(const VfsFilesystemOps *Ops, void *Filesystem);
const char *VfsRootPath(void);
const char *VfsBootPath(void);

bool FileOpen(const char *Path, File *Handle);
size_t FileRead(File *Handle, void *Buffer, size_t Length);
bool FileSeek(File *Handle, size_t Position);
size_t FileTell(const File *Handle);
size_t FileSize(const File *Handle);
bool FileIsOpen(const File *Handle);
void FileClose(File *Handle);

#endif
