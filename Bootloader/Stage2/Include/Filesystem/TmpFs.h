#ifndef FS_TMPFS_H
#define FS_TMPFS_H

#include <Filesystem/Vfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TmpFsMaxNodes 16u

typedef struct TmpFsNode {
	const char *Path;
	const uint8_t *Data;
	size_t Size;
	bool Directory;
} TmpFsNode;

typedef struct TmpFs {
	TmpFsNode *Nodes;
	size_t MaxNodesCount;
	size_t NodesCount;
} TmpFs;

bool TmpFsInit(TmpFs *Instance, size_t MaxNodesCount);
bool TmpFsAddFile(TmpFs *Instance, const char *Path, const void *Data,
				  size_t Size);
bool TmpFsAddDirectory(TmpFs *Instance, const char *Path);
const VfsFilesystemOps *TmpFsGetFilesystemOps(void);

#endif
