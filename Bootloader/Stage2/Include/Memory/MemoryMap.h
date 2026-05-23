#ifndef MEM_MEMORYMAP_H
#define MEM_MEMORYMAP_H

#include <stddef.h>
#include <stdint.h>

#define MemoryMapMaxEntries 32u
#define MemoryMapTypeUsable 1u

typedef struct MemoryMapEntry {
	uint64_t Base;
	uint64_t Length;
	uint32_t Type;
	uint32_t Attributes;
} __attribute__((packed)) MemoryMapEntry;

typedef struct MemoryMap {
	uint32_t EntriesCount;
	MemoryMapEntry Entries[MemoryMapMaxEntries];
} __attribute__((packed)) MemoryMap;

extern const MemoryMap BootMemoryMapTable;

const MemoryMap *MemoryMapGetBoot(void);
void MemoryMapLog(const MemoryMap *Map);

#endif
