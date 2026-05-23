#include <Memory/MemoryMap.h>

#include <Library/Log.h>

const MemoryMap *MemoryMapGetBoot(void)
{
	return &BootMemoryMapTable;
}

void MemoryMapLog(const MemoryMap *Map)
{
	if (Map == 0) {
		LogError("MEM", "memory map missing");
		return;
	}

	LogInfo("MEM", "E820 entries %u", (unsigned int)Map->EntriesCount);

	for (uint32_t Index = 0;
		 Index < Map->EntriesCount && Index < MemoryMapMaxEntries; ++Index) {
		const MemoryMapEntry *Entry = &Map->Entries[Index];
		LogDebug("MEM", "%u base 0x%08x len 0x%08x type %u",
				 (unsigned int)Index, (unsigned int)Entry->Base,
				 (unsigned int)Entry->Length, (unsigned int)Entry->Type);
	}
}
