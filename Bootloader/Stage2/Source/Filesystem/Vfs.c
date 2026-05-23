#include <Filesystem/Vfs.h>

#include <Library/Log.h>
#include <Memory/Allocator.h>

#define VfsMaxMounts 4u
#define VfsBootNamespace "boot"
#define VfsRootPrefix "boot:/"

typedef struct VfsMountPoint {
	char Namespace[VfsMaxNamespaceLength + 1u];
	const VfsFilesystemOps *Ops;
	void *Filesystem;
	bool Mounted;
} VfsMountPoint;

typedef struct VfsResolvedPath {
	VfsMountPoint *Mount;
	const char *LocalPath;
} VfsResolvedPath;

static VfsMountPoint *Mounts;
static size_t MountCount;

static bool EnsureMountTable(void)
{
	if (Mounts != 0) {
		return true;
	}

	Mounts = Calloc(VfsMaxMounts, sizeof(VfsMountPoint));
	if (Mounts == 0) {
		MountCount = 0;
		LogError("VFS", "failed to allocate mount table");
		return false;
	}

	MountCount = VfsMaxMounts;
	return true;
}

static bool StringEqual(const char *Left, const char *Right)
{
	if (Left == 0 || Right == 0) {
		return false;
	}

	while (*Left != '\0' && *Right != '\0') {
		if (*Left++ != *Right++) {
			return false;
		}
	}

	return *Left == '\0' && *Right == '\0';
}

static bool NamespaceEqual(const char *Path, const char *Namespace,
						   size_t NamespaceLength)
{
	size_t Index = 0;

	while (Index < NamespaceLength) {
		if (Namespace[Index] == '\0' || Path[Index] != Namespace[Index]) {
			return false;
		}

		++Index;
	}

	return Namespace[Index] == '\0';
}

static bool IsNamespaceChar(char Character)
{
	return (Character >= 'a' && Character <= 'z') ||
		   (Character >= 'A' && Character <= 'Z') ||
		   (Character >= '0' && Character <= '9') || Character == '_' ||
		   Character == '-';
}

static bool CopyNamespace(char Out[VfsMaxNamespaceLength + 1u],
						  const char *Namespace)
{
	size_t Index = 0;

	if (Namespace == 0 || Namespace[0] == '\0') {
		return false;
	}

	while (Namespace[Index] != '\0') {
		if (Index >= VfsMaxNamespaceLength ||
			!IsNamespaceChar(Namespace[Index])) {
			return false;
		}

		Out[Index] = Namespace[Index];
		++Index;
	}

	Out[Index] = '\0';
	return true;
}

static bool ParsePath(const char *Path, const char **NamespaceEndOut,
					  const char **LocalPathOut)
{
	const char *Cursor = Path;

	if (Path == 0 || !IsNamespaceChar(*Cursor)) {
		return false;
	}

	while (IsNamespaceChar(*Cursor)) {
		++Cursor;
	}

	if (Cursor[0] != ':' || Cursor[1] != '/') {
		return false;
	}

	*NamespaceEndOut = Cursor;
	*LocalPathOut = Cursor + 2;
	return true;
}

static bool ResolvePath(const char *Path, VfsResolvedPath *Resolved)
{
	const char *NamespaceEnd;
	const char *LocalPath;

	if (!EnsureMountTable()) {
		return false;
	}

	if (!ParsePath(Path, &NamespaceEnd, &LocalPath)) {
		LogWarn("VFS", "unsupported path '%s'", Path != 0 ? Path : "");
		return false;
	}

	size_t NamespaceLength = (size_t)(NamespaceEnd - Path);

	for (size_t Index = 0; Index < MountCount; ++Index) {
		if (!Mounts[Index].Mounted) {
			continue;
		}

		if (NamespaceEqual(Path, Mounts[Index].Namespace, NamespaceLength)) {
			Resolved->Mount = &Mounts[Index];
			Resolved->LocalPath = LocalPath;
			return true;
		}
	}

	LogWarn("VFS", "namespace not mounted in '%s'", Path);
	return false;
}

bool VfsMount(const char *Namespace, const VfsFilesystemOps *Ops,
			  void *Filesystem)
{
	char SanitizedNamespace[VfsMaxNamespaceLength + 1u];
	VfsMountPoint *FreeMount = 0;

	if (!EnsureMountTable()) {
		return false;
	}

	if (!CopyNamespace(SanitizedNamespace, Namespace) || Ops == 0 ||
		Ops->Open == 0 || Ops->Read == 0 || Filesystem == 0) {
		LogError("VFS", "mount rejected: invalid argument");
		return false;
	}

	for (size_t Index = 0; Index < MountCount; ++Index) {
		if (Mounts[Index].Mounted &&
			StringEqual(Mounts[Index].Namespace, SanitizedNamespace)) {
			Mounts[Index].Ops = Ops;
			Mounts[Index].Filesystem = Filesystem;
			LogDebug("VFS", "remounted %s:/", SanitizedNamespace);
			return true;
		}

		if (!Mounts[Index].Mounted && FreeMount == 0) {
			FreeMount = &Mounts[Index];
		}
	}

	if (FreeMount == 0) {
		LogError("VFS", "mount table full");
		return false;
	}

	for (size_t Index = 0; Index <= VfsMaxNamespaceLength; ++Index) {
		FreeMount->Namespace[Index] = SanitizedNamespace[Index];
		if (SanitizedNamespace[Index] == '\0') {
			break;
		}
	}

	FreeMount->Ops = Ops;
	FreeMount->Filesystem = Filesystem;
	FreeMount->Mounted = true;
	LogDebug("VFS", "mounted %s:/", SanitizedNamespace);
	return true;
}

bool VfsMountRoot(const VfsFilesystemOps *Ops, void *Filesystem)
{
	return VfsMount(VfsBootNamespace, Ops, Filesystem);
}

const char *VfsRootPath(void)
{
	return VfsRootPrefix;
}

const char *VfsBootPath(void)
{
	return VfsRootPrefix;
}

bool FileOpen(const char *Path, File *Handle)
{
	VfsResolvedPath Resolved;

	if (Handle == 0) {
		LogError("VFS", "open rejected: null file handle");
		return false;
	}

	FileClose(Handle);

	if (!ResolvePath(Path, &Resolved)) {
		return false;
	}

	if (!Resolved.Mount->Ops->Open(Resolved.Mount->Filesystem,
								   Resolved.LocalPath, &Handle->Node)) {
		LogWarn("VFS", "file not found '%s'", Path);
		return false;
	}

	if (Handle->Node.Directory) {
		LogWarn("VFS", "path is a directory '%s'", Path);
		return false;
	}

	Handle->Ops = Resolved.Mount->Ops;
	Handle->Filesystem = Resolved.Mount->Filesystem;
	Handle->Position = 0;
	Handle->Open = true;
	LogDebug("VFS", "opened '%s' (%u bytes)", Path,
			 (unsigned int)Handle->Node.Size);
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
	if (!FileIsOpen(Handle)) {
		LogWarn("VFS", "seek rejected: file not open");
		return false;
	}

	if (Position > Handle->Node.Size) {
		LogWarn("VFS", "seek past EOF: %u > %u", (unsigned int)Position,
				(unsigned int)Handle->Node.Size);
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

	return Handle->Node.Size;
}

bool FileIsOpen(const File *Handle)
{
	return Handle != 0 && Handle->Open;
}

void FileClose(File *Handle)
{
	if (Handle == 0) {
		return;
	}

	Handle->Ops = 0;
	Handle->Filesystem = 0;
	Handle->Node.Alignment = 0;
	for (size_t Index = 0; Index < VfsNodeStorageSize; ++Index) {
		Handle->Node.Storage[Index] = 0;
	}
	Handle->Node.Size = 0;
	Handle->Node.Directory = false;
	Handle->Position = 0;
	Handle->Open = false;
}
