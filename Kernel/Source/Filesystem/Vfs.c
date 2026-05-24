#include <Filesystem/Vfs.h>

#include <Core/Log.h>
#include <Memory/Heap.h>

#define VfsFileMagic 0x10101010u
#define VfsDirMagic 0x20202020u

typedef struct VfsVolume {
	char Id;
	const VfsFilesystemOps *Ops;
	void *Filesystem;
	struct VfsVolume *Next;
} VfsVolume;

static VfsVolume *VolumeList;

static char NormalizeVolumeId(char VolumeId)
{
	if (VolumeId >= 'A' && VolumeId <= 'Z') {
		return (char)(VolumeId - 'A' + 'a');
	}

	return VolumeId;
}

static bool IsValidVolumeId(char VolumeId)
{
	VolumeId = NormalizeVolumeId(VolumeId);
	return VolumeId >= 'a' && VolumeId <= 'z';
}

static void VfsClearNode(VfsNode *Node)
{
	if (Node == 0) {
		return;
	}

	Node->Flags = 0;
	Node->PrivateData = 0;
}

static void VfsClearStat(VfsStat *Stat)
{
	if (Stat == 0) {
		return;
	}

	Stat->Flags = 0;
	Stat->Size = 0;
}

static void VfsCloseNode(const VfsFilesystemOps *Ops, void *Filesystem,
						 VfsNode *Node)
{
	if (Ops != 0 && Ops->Close != 0 && Node != 0 && Node->PrivateData != 0) {
		Ops->Close(Filesystem, Node);
	}

	VfsClearNode(Node);
}

static VfsVolume *FindVolume(char VolumeId)
{
	VolumeId = NormalizeVolumeId(VolumeId);

	for (VfsVolume *Volume = VolumeList; Volume != 0; Volume = Volume->Next) {
		if (Volume->Id == VolumeId) {
			return Volume;
		}
	}

	return 0;
}

static bool ParseVolumePath(const char *Path, char *VolumeIdOut,
							const char **LocalPathOut)
{
	if (Path == 0 || VolumeIdOut == 0 || LocalPathOut == 0) {
		return false;
	}

	if (!IsValidVolumeId(Path[0]) || Path[1] != ':' || Path[2] != '/') {
		return false;
	}

	*VolumeIdOut = NormalizeVolumeId(Path[0]);
	*LocalPathOut = &Path[2];
	return true;
}

static bool OpenPath(const char *Path, VfsNode *Node, VfsVolume **VolumeOut)
{
	char VolumeId;
	const char *LocalPath;
	VfsVolume *Volume;

	if (Node == 0 || VolumeOut == 0) {
		return false;
	}

	if (!ParseVolumePath(Path, &VolumeId, &LocalPath)) {
		LogWarn("fs.vfs", "unsupported path '%s'", Path != 0 ? Path : "");
		return false;
	}

	Volume = FindVolume(VolumeId);
	if (Volume == 0) {
		LogWarn("fs.vfs", "unknown volume '%c:'", VolumeId);
		return false;
	}

	if (!Volume->Ops->Open(Volume->Filesystem, LocalPath, Node)) {
		return false;
	}

	*VolumeOut = Volume;
	return true;
}

bool VfsMount(char VolumeId, const VfsFilesystemOps *Ops, void *Filesystem)
{
	VfsVolume *Volume;

	VolumeId = NormalizeVolumeId(VolumeId);
	if (!IsValidVolumeId(VolumeId) || Ops == 0 || Ops->Open == 0 ||
		Ops->Close == 0 || Ops->Stat == 0 || Ops->Read == 0 ||
		Ops->ReadDir == 0 || Filesystem == 0) {
		LogError("fs.vfs", "mount rejected: invalid argument");
		return false;
	}

	if (FindVolume(VolumeId) != 0) {
		LogWarn("fs.vfs", "volume already mounted '%c:'", VolumeId);
		return false;
	}

	Volume = KernelZalloc(sizeof(*Volume));
	if (Volume == 0) {
		LogError("fs.vfs", "volume allocation failed");
		return false;
	}

	Volume->Id = VolumeId;
	Volume->Ops = Ops;
	Volume->Filesystem = Filesystem;
	Volume->Next = VolumeList;
	VolumeList = Volume;

	LogOk("fs.vfs", "mounted %c:/", VolumeId);
	return true;
}

size_t VfsGetVolumeCount(void)
{
	size_t Count = 0;

	for (const VfsVolume *Volume = VolumeList; Volume != 0;
		 Volume = Volume->Next) {
		++Count;
	}

	return Count;
}

bool VfsGetVolumeInfo(size_t Index, VfsVolumeInfo *Out)
{
	size_t Current = 0;

	if (Out == 0) {
		return false;
	}

	for (const VfsVolume *Volume = VolumeList; Volume != 0;
		 Volume = Volume->Next) {
		if (Current == Index) {
			Out->Id = Volume->Id;
			Out->Filesystem = Volume->Filesystem;
			Out->Ops = Volume->Ops;
			return true;
		}

		++Current;
	}

	return false;
}

bool FileOpen(const char *Path, File *Handle)
{
	VfsVolume *Volume;

	if (Handle == 0) {
		return false;
	}

	if (Handle->Magic == VfsFileMagic && Handle->Open) {
		FileClose(Handle);
	} else {
		Handle->Ops = 0;
		Handle->Filesystem = 0;
		VfsClearNode(&Handle->Node);
		VfsClearStat(&Handle->Stat);
		Handle->Magic = VfsFileMagic;
		Handle->Position = 0;
		Handle->Open = false;
	}

	if (!OpenPath(Path, &Handle->Node, &Volume)) {
		return false;
	}

	if (!Volume->Ops->Stat(Volume->Filesystem, &Handle->Node, &Handle->Stat)) {
		VfsCloseNode(Volume->Ops, Volume->Filesystem, &Handle->Node);
		return false;
	}

	Handle->Node.Flags = Handle->Stat.Flags;
	if ((Handle->Node.Flags & VfsNodeFlagDirectory) != 0) {
		LogWarn("fs.vfs", "path is a directory '%s'", Path);
		VfsCloseNode(Volume->Ops, Volume->Filesystem, &Handle->Node);
		return false;
	}

	Handle->Ops = Volume->Ops;
	Handle->Filesystem = Volume->Filesystem;
	Handle->Magic = VfsFileMagic;
	Handle->Position = 0;
	Handle->Open = true;
	return true;
}

size_t FileRead(File *Handle, void *Buffer, size_t Length)
{
	if (!FileIsOpen(Handle) || Buffer == 0 || Length == 0) {
		return 0;
	}

	size_t Read = Handle->Ops->Read(Handle->Filesystem, &Handle->Node,
									Handle->Position, Buffer, Length);
	Handle->Position += Read;
	return Read;
}

bool FileSeek(File *Handle, size_t Position)
{
	if (!FileIsOpen(Handle) || Position > Handle->Stat.Size) {
		return false;
	}

	Handle->Position = Position;
	return true;
}

size_t FileTell(const File *Handle)
{
	if (!FileIsOpen(Handle)) {
		return 0;
	}

	return Handle->Position;
}

size_t FileSize(const File *Handle)
{
	if (!FileIsOpen(Handle)) {
		return 0;
	}

	return Handle->Stat.Size;
}

bool FileStat(const File *Handle, VfsStat *Out)
{
	if (!FileIsOpen(Handle) || Out == 0) {
		return false;
	}

	*Out = Handle->Stat;
	return true;
}

bool FileIsOpen(const File *Handle)
{
	return Handle != 0 && Handle->Magic == VfsFileMagic && Handle->Open;
}

void FileClose(File *Handle)
{
	if (Handle == 0) {
		return;
	}

	if (Handle->Magic == VfsFileMagic && Handle->Open) {
		VfsCloseNode(Handle->Ops, Handle->Filesystem, &Handle->Node);
	} else {
		VfsClearNode(&Handle->Node);
	}

	Handle->Ops = 0;
	Handle->Filesystem = 0;
	VfsClearStat(&Handle->Stat);
	Handle->Magic = VfsFileMagic;
	Handle->Position = 0;
	Handle->Open = false;
}

bool DirOpen(const char *Path, Directory *Handle)
{
	VfsVolume *Volume;

	if (Handle == 0) {
		return false;
	}

	if (Handle->Magic == VfsDirMagic && Handle->Open) {
		DirClose(Handle);
	} else {
		Handle->Ops = 0;
		Handle->Filesystem = 0;
		VfsClearNode(&Handle->Node);
		VfsClearStat(&Handle->Stat);
		Handle->Magic = VfsDirMagic;
		Handle->Index = 0;
		Handle->Open = false;
	}

	if (!OpenPath(Path, &Handle->Node, &Volume)) {
		return false;
	}

	if (!Volume->Ops->Stat(Volume->Filesystem, &Handle->Node, &Handle->Stat)) {
		VfsCloseNode(Volume->Ops, Volume->Filesystem, &Handle->Node);
		return false;
	}

	Handle->Node.Flags = Handle->Stat.Flags;
	if ((Handle->Node.Flags & VfsNodeFlagDirectory) == 0) {
		LogWarn("fs.vfs", "path is not a directory '%s'", Path);
		VfsCloseNode(Volume->Ops, Volume->Filesystem, &Handle->Node);
		return false;
	}

	Handle->Ops = Volume->Ops;
	Handle->Filesystem = Volume->Filesystem;
	Handle->Magic = VfsDirMagic;
	Handle->Index = 0;
	Handle->Open = true;
	return true;
}

bool DirRead(Directory *Handle, VfsDirent *Out)
{
	if (!DirIsOpen(Handle) || Out == 0) {
		return false;
	}

	if (!Handle->Ops->ReadDir(Handle->Filesystem, &Handle->Node, Handle->Index,
							  Out)) {
		return false;
	}

	Handle->Index++;
	return true;
}

bool DirStat(const Directory *Handle, VfsStat *Out)
{
	if (!DirIsOpen(Handle) || Out == 0) {
		return false;
	}

	*Out = Handle->Stat;
	return true;
}

bool DirIsOpen(const Directory *Handle)
{
	return Handle != 0 && Handle->Magic == VfsDirMagic && Handle->Open;
}

void DirClose(Directory *Handle)
{
	if (Handle == 0) {
		return;
	}

	if (Handle->Magic == VfsDirMagic && Handle->Open) {
		VfsCloseNode(Handle->Ops, Handle->Filesystem, &Handle->Node);
	} else {
		VfsClearNode(&Handle->Node);
	}

	Handle->Ops = 0;
	Handle->Filesystem = 0;
	VfsClearStat(&Handle->Stat);
	Handle->Magic = VfsDirMagic;
	Handle->Index = 0;
	Handle->Open = false;
}

bool VfsStatPath(const char *Path, VfsStat *Out)
{
	VfsVolume *Volume;
	VfsNode Node;
	bool Ok;

	if (Out == 0) {
		return false;
	}

	VfsClearNode(&Node);
	VfsClearStat(Out);
	if (!OpenPath(Path, &Node, &Volume)) {
		return false;
	}

	Ok = Volume->Ops->Stat(Volume->Filesystem, &Node, Out);
	VfsCloseNode(Volume->Ops, Volume->Filesystem, &Node);
	return Ok;
}
