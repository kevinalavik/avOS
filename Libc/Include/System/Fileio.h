#ifndef SYSTEM_FILEIO_H
#define SYSTEM_FILEIO_H

#include <System/Types.h>

typedef struct DirEntry {
	char Name[128];
	U64 Size;
	U32 Flags;
} DirEntry;

Handle FileOpen(const char *Path);
void FileClose(Handle File);
SSize FileRead(Handle File, void *Buffer, Size Length);
SSize FileWrite(Handle File, const void *Buffer, Size Length);
SSize FileSeek(Handle File, SSize Offset, U32 Whence);
Offset FileSize(Handle File);

Handle DirOpen(const char *Path);
void DirClose(Handle Dir);
int DirRead(Handle Dir, DirEntry *Entry);

#endif
