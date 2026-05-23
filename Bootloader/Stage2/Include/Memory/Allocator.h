#ifndef MEM_ALLOCATOR_H
#define MEM_ALLOCATOR_H

#include <Memory/MemoryMap.h>

#include <stdbool.h>
#include <stddef.h>

bool AllocatorInit(const MemoryMap *Map);
void *Alloc(size_t Size, size_t Alignment);
void *Calloc(size_t ItemsCount, size_t Item);
size_t AllocatorFreeBytes(void);

#endif
