#include <Drivers/Storage/Disk.h>

#include <Core/Log.h>
#include <Drivers/Storage/Ata.h>

typedef struct DiskEntry {
	DiskInfo Info;
	BlockDevice Device;
	const AtaDevice *Ata;
	char Name[8];
	char Description[48];
} DiskEntry;

static DiskEntry DiskEntries[DiskMaxCount];
static size_t DiskCount;

static bool DiskReadSectors(void *Context, uint32_t Lba, uint8_t Count,
							void *Buffer)
{
	const DiskEntry *Entry = (const DiskEntry *)Context;

	if (Entry == 0 || Entry->Ata == 0) {
		return false;
	}

	return AtaPioRead28(Entry->Ata, Lba, Count, Buffer);
}

static void DiskSetName(char Out[8], size_t Index)
{
	Out[0] = 'd';
	Out[1] = 'i';
	Out[2] = 's';
	Out[3] = 'k';
	Out[4] = (char)('0' + Index);
	Out[5] = '\0';
}

static void DiskSetDescription(char Out[48], const AtaDevice *Ata)
{
	const char Prefix[] = "ATA ";
	size_t Write = 0;

	for (size_t Index = 0; Prefix[Index] != '\0'; ++Index) {
		Out[Write++] = Prefix[Index];
	}

	for (size_t Index = 0;
		 Ata != 0 && Ata->Name[Index] != '\0' && Write + 1 < 48; ++Index) {
		Out[Write++] = Ata->Name[Index];
	}

	Out[Write] = '\0';
}

bool DiskInit(void)
{
	if (!AtaInit()) {
		LogWarn("dev.disk", "ATA unavailable");
		return false;
	}

	DiskCount = 0;
	for (size_t Index = 0;
		 Index < AtaGetDeviceCount() && DiskCount < DiskMaxCount; ++Index) {
		const AtaDevice *Ata = AtaGetDevice(Index);
		DiskEntry *Entry = &DiskEntries[DiskCount];

		DiskSetName(Entry->Name, DiskCount);
		DiskSetDescription(Entry->Description, Ata);

		Entry->Ata = Ata;
		Entry->Device.Context = Entry;
		Entry->Device.ReadSectors = DiskReadSectors;
		Entry->Info.VolumeId = (char)('a' + DiskCount);
		Entry->Info.Name = Entry->Name;
		Entry->Info.Description = Entry->Description;
		Entry->Info.Device = &Entry->Device;
		Entry->Info.Boot = DiskCount == 0;

		LogOk("dev.disk", "%s ready as %c:/ (%s)", Entry->Info.Name,
			  Entry->Info.VolumeId, Entry->Info.Description);
		++DiskCount;
	}

	if (DiskCount == 0) {
		LogWarn("dev.disk", "no disks registered");
		return false;
	}

	return true;
}

size_t DiskGetCount(void)
{
	return DiskCount;
}

const DiskInfo *DiskGetInfo(size_t Index)
{
	if (Index >= DiskCount) {
		return 0;
	}

	return &DiskEntries[Index].Info;
}

const DiskInfo *DiskGetBootDisk(void)
{
	return DiskCount > 0 ? &DiskEntries[0].Info : 0;
}
