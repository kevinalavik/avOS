#include <Filesystem/Vfs.h>

#include <Library/DebugLog.h>

#define VfsRootPrefix "/"

static const VfsFilesystemOps *RootOps;
static void *RootFilesystem;
static bool RootMounted;

bool VfsMountRoot(const VfsFilesystemOps *Ops, void *Filesystem)
{
	if (Ops == 0 || Ops->Open == 0 || Ops->Read == 0 || Filesystem == 0) {
		BootError("VFS", "root mount rejected: invalid argument");
		return false;
	}

	RootOps = Ops;
	RootFilesystem = Filesystem;
	RootMounted = true;
	DebugLog("VFS", "mounted root /");
	return true;
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
	if (Handle == 0) {
		BootError("VFS", "open rejected: null file handle");
		return false;
	}

	FileClose(Handle);

	if (!RootMounted) {
		BootError("VFS", "open rejected: no root filesystem");
		return false;
	}

	if (Path == 0 || Path[0] != '/') {
		DebugLog("VFS", "unsupported path '%s'", Path != 0 ? Path : "");
		return false;
	}

	if (!RootOps->Open(RootFilesystem, Path, &Handle->Node)) {
		DebugLog("VFS", "file not found '%s'", Path);
		return false;
	}

	if (Handle->Node.Directory) {
		DebugLog("VFS", "path is a directory '%s'", Path);
		return false;
	}

	Handle->Ops = RootOps;
	Handle->Filesystem = RootFilesystem;
	Handle->Position = 0;
	Handle->Open = true;
	DebugLog("VFS", "opened '%s' (%u bytes)", Path,
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
		DebugLog("VFS", "seek rejected: file not open");
		return false;
	}

	if (Position > Handle->Node.Size) {
		DebugLog("VFS", "seek past EOF: %u > %u", (unsigned int)Position,
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
