#ifndef MEMORY_HEAP_H
#define MEMORY_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KernelHeapReserveSize 0x4000000ull
#define KernelHeapInitialPages 16ull

bool HeapInit(void);
void *HeapAlloc(size_t Size, size_t Alignment);
void *HeapCalloc(size_t ItemsCount, size_t ItemSize, size_t Alignment);
void *HeapRealloc(void *Pointer, size_t Size, size_t Alignment);
void HeapFree(void *Pointer);

void *KernelAlloc(size_t Size);
void *KernelAllocAligned(size_t Size, size_t Alignment);
void *KernelZalloc(size_t Size);
void *KernelCalloc(size_t ItemsCount, size_t ItemSize);
void *KernelRealloc(void *Pointer, size_t Size);
void KernelFree(void *Pointer);

uint64_t HeapGetBase(void);
uint64_t HeapGetUsedBytes(void);
uint64_t HeapGetFreeBytes(void);
uint64_t HeapGetCommittedBytes(void);
uint64_t HeapGetReservedBytes(void);
uint64_t HeapGetAllocCount(void);
uint64_t HeapGetFreeCount(void);

#endif
