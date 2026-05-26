#ifndef SYSTEM_MEMORY_H
#define SYSTEM_MEMORY_H

#include <System/Types.h>

void *MemoryAlloc(Size Bytes);
void MemoryFree(void *Pointer);
void *MemoryRealloc(void *Pointer, Size Bytes);
void *MemoryCalloc(Size Num, Size Size);

// posix complaint
void *malloc(Size Bytes);
void free(void *Pointer);
void *realloc(void *Pointer, Size Bytes);
void *calloc(Size Num, Size Size);
void *zalloc(Size Bytes);

void *Map(Size Length, U32 Flags);
void Unmap(void *Address, Size Length);

U64 SharedMemCreate(Size Size);
void *SharedMemMap(U64 Id);
void SharedMemUnmap(void *Address);
void SharedMemDestroy(U64 Id);

#endif