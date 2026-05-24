#include <Memory/MemoryMap.h>

#include <Library/DebugLog.h>

const MemoryMap *MemoryMapGetBoot(void)
{
	return &BootMemoryMapTable;
}

void MemoryMapLog(const MemoryMap *Map)
{
	if (Map == 0) {
		BootError("MEM", "memory map missing");
		return;
	}

	DebugLog("MEM", "E820 entries %u", (unsigned int)Map->EntriesCount);

	for (uint32_t Index = 0;
		 Index < Map->EntriesCount && Index < MemoryMapMaxEntries; ++Index) {
		const MemoryMapEntry *Entry = &Map->Entries[Index];
		DebugLog("MEM", "%u base 0x%08x len 0x%08x type %u",
				 (unsigned int)Index, (unsigned int)Entry->Base,
				 (unsigned int)Entry->Length, (unsigned int)Entry->Type);
	}
}
