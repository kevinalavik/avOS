#include <Filesystem/TmpFs.h>

#include <Memory/Allocator.h>

static bool PathEqual(const char *Left, const char *Right)
{
	if (Left == 0 || Right == 0) {
		return false;
	}

	while (*Left == '/') {
		++Left;
	}

	while (*Right == '/') {
		++Right;
	}

	while (*Left != '\0' && *Right != '\0') {
		if (*Left++ != *Right++) {
			return false;
		}
	}

	while (*Left == '/') {
		++Left;
	}

	while (*Right == '/') {
		++Right;
	}

	return *Left == '\0' && *Right == '\0';
}

bool TmpFsInit(TmpFs *Instance, size_t MaxNodesCount)
{
	if (Instance == 0) {
		return false;
	}

	if (MaxNodesCount == 0) {
		MaxNodesCount = TmpFsMaxNodes;
	}

	Instance->Nodes = Calloc(MaxNodesCount, sizeof(TmpFsNode));
	if (Instance->Nodes == 0) {
		Instance->MaxNodesCount = 0;
		Instance->NodesCount = 0;
		return false;
	}

	Instance->MaxNodesCount = MaxNodesCount;
	Instance->NodesCount = 0;
	return true;
}

static bool TmpFsAddNode(TmpFs *Instance, const char *Path, const void *Data,
						 size_t Size, bool Directory)
{
	if (Instance == 0 || Path == 0 || Instance->Nodes == 0 ||
		Instance->NodesCount >= Instance->MaxNodesCount) {
		return false;
	}

	TmpFsNode *Node = &Instance->Nodes[Instance->NodesCount++];
	Node->Path = Path;
	Node->Data = Data;
	Node->Size = Directory ? 0 : Size;
	Node->Directory = Directory;
	return true;
}

bool TmpFsAddFile(TmpFs *Instance, const char *Path, const void *Data,
				  size_t Size)
{
	if (Data == 0 && Size != 0) {
		return false;
	}

	return TmpFsAddNode(Instance, Path, Data, Size, false);
}

bool TmpFsAddDirectory(TmpFs *Instance, const char *Path)
{
	return TmpFsAddNode(Instance, Path, 0, 0, true);
}

static bool TmpFsOpen(void *Filesystem, const char *Path, VfsNode *Node)
{
	if (Filesystem == 0 || Node == 0) {
		return false;
	}

	TmpFs *Instance = Filesystem;
	for (size_t Index = 0; Index < Instance->NodesCount; ++Index) {
		if (!PathEqual(Path, Instance->Nodes[Index].Path)) {
			continue;
		}

		const TmpFsNode **NodeOut = (const TmpFsNode **)Node->Storage;
		*NodeOut = &Instance->Nodes[Index];
		Node->Size = Instance->Nodes[Index].Size;
		Node->Directory = Instance->Nodes[Index].Directory;
		return true;
	}

	return false;
}

static size_t TmpFsRead(void *Filesystem, const VfsNode *Node, size_t Offset,
						void *Buffer, size_t Length)
{
	(void)Filesystem;

	if (Node == 0 || Buffer == 0 || Length == 0) {
		return 0;
	}

	const TmpFsNode *TmpNode = *(const TmpFsNode *const *)Node->Storage;
	if (TmpNode == 0 || TmpNode->Directory || Offset >= TmpNode->Size) {
		return 0;
	}

	size_t Available = TmpNode->Size - Offset;
	size_t Read = Length < Available ? Length : Available;
	uint8_t *Out = Buffer;
	const uint8_t *In = TmpNode->Data + Offset;

	for (size_t Index = 0; Index < Read; ++Index) {
		Out[Index] = In[Index];
	}

	return Read;
}

static const VfsFilesystemOps TmpFsFilesystemOps = {
	.Open = TmpFsOpen,
	.Read = TmpFsRead,
};

const VfsFilesystemOps *TmpFsGetFilesystemOps(void)
{
	return &TmpFsFilesystemOps;
}
