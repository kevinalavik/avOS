#include <System/Fileio.h>
#include <System/Syscall.h>

Handle FileOpen(const char *Path)
{
	return (Handle)Syscall1(SyscallFileOpen, (U64)Path);
}

void FileClose(Handle File)
{
	Syscall1(SyscallFileClose, File);
}

SSize FileRead(Handle File, void *Buffer, Size Length)
{
	return (SSize)Syscall3(SyscallFileRead, File, (U64)Buffer, Length);
}

SSize FileWrite(Handle File, const void *Buffer, Size Length)
{
	return (SSize)Syscall3(SyscallFileWrite, File, (U64)Buffer, Length);
}

SSize FileSeek(Handle File, SSize Offset, U32 Whence)
{
	return (SSize)Syscall3(SyscallFileSeek, File, (U64)Offset, Whence);
}

Offset FileSize(Handle File)
{
	return (Offset)Syscall1(SyscallFileSize, File);
}

Handle DirOpen(const char *Path)
{
	return (Handle)Syscall1(SyscallDirOpen, (U64)Path);
}

void DirClose(Handle Dir)
{
	Syscall1(SyscallDirClose, Dir);
}

int DirRead(Handle Dir, DirEntry *Entry)
{
	return (int)Syscall2(SyscallDirRead, Dir, (U64)Entry);
}
