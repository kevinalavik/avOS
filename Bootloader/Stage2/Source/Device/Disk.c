#include <Device/Disk.h>
#include <Device/PortIO.h>
#include <Library/Log.h>

#define AtaData 0x1F0
#define AtaSectorCount 0x1F2
#define AtaLbaLow 0x1F3
#define AtaLbaMid 0x1F4
#define AtaLbaHigh 0x1F5
#define AtaDrive 0x1F6
#define AtaStatusCommand 0x1F7
#define AtaAltStatus 0x3F6

#define AtaCommandReadSectors 0x20
#define AtaDriveMasterLba 0xE0

#define AtaStatusError 0x01
#define AtaStatusDataRequest 0x08
#define AtaStatusDeviceFault 0x20
#define AtaStatusBusy 0x80

#define AtaTimeout 100000u

static void IoWait(void)
{
	(void)PortIORead(PortIOWidth8, AtaAltStatus);
	(void)PortIORead(PortIOWidth8, AtaAltStatus);
	(void)PortIORead(PortIOWidth8, AtaAltStatus);
	(void)PortIORead(PortIOWidth8, AtaAltStatus);
}

static bool WaitNotBusy(void)
{
	for (uint32_t Attempt = 0; Attempt < AtaTimeout; ++Attempt) {
		if ((PortIORead(PortIOWidth8, AtaStatusCommand) & AtaStatusBusy) == 0) {
			return true;
		}
	}

	return false;
}

static bool WaitDataRequest(void)
{
	for (uint32_t Attempt = 0; Attempt < AtaTimeout; ++Attempt) {
		uint8_t Status = (uint8_t)PortIORead(PortIOWidth8, AtaStatusCommand);

		if ((Status & (AtaStatusError | AtaStatusDeviceFault)) != 0) {
			LogError("ATA", "ATA status error 0x%x", (unsigned int)Status);
			return false;
		}

		if ((Status & AtaStatusDataRequest) != 0) {
			return true;
		}
	}

	LogError("ATA", "timed out waiting for ATA data request");
	return false;
}

bool DiskReadSectors(uint32_t Lba, uint8_t Count, void *Buffer)
{
	if (Buffer == 0) {
		LogError("ATA", "read rejected: null buffer");
		return false;
	}

	if (Count == 0 || Lba > (0x10000000u - Count)) {
		LogError("ATA", "read rejected: LBA %u count %u", (unsigned int)Lba,
				 (unsigned int)Count);
		return false;
	}

	uint8_t *Out = Buffer;

	for (uint8_t Sector = 0; Sector < Count; ++Sector) {
		uint32_t CurrentLba = Lba + Sector;

		if (!WaitNotBusy()) {
			LogError("ATA", "timed out waiting for ATA idle at LBA %u",
					 (unsigned int)CurrentLba);
			return false;
		}

		PortIOWrite(PortIOWidth8, AtaDrive,
					AtaDriveMasterLba | ((CurrentLba >> 24) & 0x0F));
		IoWait();
		PortIOWrite(PortIOWidth8, AtaSectorCount, 1);
		PortIOWrite(PortIOWidth8, AtaLbaLow, CurrentLba & 0xFF);
		PortIOWrite(PortIOWidth8, AtaLbaMid, (CurrentLba >> 8) & 0xFF);
		PortIOWrite(PortIOWidth8, AtaLbaHigh, (CurrentLba >> 16) & 0xFF);
		PortIOWrite(PortIOWidth8, AtaStatusCommand, AtaCommandReadSectors);

		if (!WaitDataRequest()) {
			LogError("ATA", "ATA read failed at LBA %u",
					 (unsigned int)CurrentLba);
			return false;
		}

		for (uint16_t Word = 0; Word < DiskSectorSize / 2u; ++Word) {
			uint16_t Value = (uint16_t)PortIORead(PortIOWidth16, AtaData);
			*Out++ = (uint8_t)(Value & 0xFF);
			*Out++ = (uint8_t)(Value >> 8);
		}
	}

	return true;
}

bool DiskInit(void)
{
	PortIOWrite(PortIOWidth8, AtaDrive, AtaDriveMasterLba);
	IoWait();

	if (!WaitNotBusy()) {
		LogError("ATA", "ATA master did not become ready");
		return false;
	}

	LogDebug("ATA", "ATA master selected");
	return true;
}

static const DiskDevice DiskDeviceInstance = {
	.ReadSectors = DiskReadSectors,
};

const DiskDevice *DiskGetDevice(void)
{
	return &DiskDeviceInstance;
}
