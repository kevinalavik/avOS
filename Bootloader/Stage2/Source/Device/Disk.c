#include <Device/Disk.h>
#include <Library/DebugLog.h>

#include <stddef.h>

extern uint8_t Stage2BootDrive;
uint8_t BiosDiskReadSectors(uint32_t Lba, uint8_t Count, void *Buffer);

static uint8_t BiosBounceSector[DiskSectorSize] __attribute__((aligned(16)));

static void CopyBytes(void *Destination, const void *Source, size_t Size)
{
	uint8_t *Out = Destination;
	const uint8_t *In = Source;

	for (size_t Index = 0; Index < Size; ++Index) {
		Out[Index] = In[Index];
	}
}

bool DiskReadSectors(uint32_t Lba, uint8_t Count, void *Buffer)
{
	if (Buffer == 0) {
		BootError("BIOS", "read rejected: null buffer");
		return false;
	}

	if (Count == 0 || Lba > (UINT32_MAX - Count)) {
		BootError("BIOS", "read rejected: LBA %u count %u", (unsigned int)Lba,
				  (unsigned int)Count);
		return false;
	}

	if ((uintptr_t)BiosBounceSector + sizeof(BiosBounceSector) > 0x100000u) {
		BootError("BIOS", "bounce sector outside real-mode address space");
		return false;
	}

	uint8_t *Out = Buffer;
	for (uint8_t Sector = 0; Sector < Count; ++Sector) {
		uint32_t CurrentLba = Lba + Sector;

		if (BiosDiskReadSectors(CurrentLba, 1, BiosBounceSector) == 0) {
			BootError("BIOS", "INT 13h read failed at LBA %u",
					  (unsigned int)CurrentLba);
			return false;
		}

		CopyBytes(Out, BiosBounceSector, DiskSectorSize);
		Out += DiskSectorSize;
	}

	return true;
}

bool DiskInit(void)
{
	DebugLog("BIOS", "boot drive 0x%x", (unsigned int)Stage2BootDrive);
	return true;
}

static const DiskDevice DiskDeviceInstance = {
	.ReadSectors = DiskReadSectors,
};

const DiskDevice *DiskGetDevice(void)
{
	return &DiskDeviceInstance;
}
