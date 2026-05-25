#include <Drivers/Storage/Ata.h>

#include <Core/Log.h>
#include <Device/Pit.h>
#include <Device/PortIO.h>

#include <stddef.h>

enum {
	AtaPrimaryIo = 0x1F0,
	AtaPrimaryCtrl = 0x3F6,
	AtaSecondaryIo = 0x170,
	AtaSecondaryCtrl = 0x376,
};

enum {
	AtaRegData = 0,
	AtaRegError = 1,
	AtaRegFeatures = 1,
	AtaRegSecCount0 = 2,
	AtaRegLba0 = 3,
	AtaRegLba1 = 4,
	AtaRegLba2 = 5,
	AtaRegHddevsel = 6,
	AtaRegCommand = 7,
	AtaRegStatus = 7,
};

enum {
	AtaCmdIdentify = 0xEC,
	AtaCmdReadSectors = 0x20,
};

enum {
	AtaDriveMaster = 0,
	AtaDriveSlave = 1,
};

enum {
	AtaStatusErr = 1u << 0,
	AtaStatusDrq = 1u << 3,
	AtaStatusSrv = 1u << 4,
	AtaStatusDf = 1u << 5,
	AtaStatusRdy = 1u << 6,
	AtaStatusBsy = 1u << 7,
};

typedef struct AtaProbeTarget {
	uint16_t IoBase;
	uint16_t CtrlBase;
	uint8_t Drive;
	const char *Name;
} AtaProbeTarget;

#define AtaMaxDevices 4u

static AtaDevice AtaDevices[AtaMaxDevices];
static size_t AtaDeviceCount;

static const AtaProbeTarget AtaProbeTargets[] = {
	{ AtaPrimaryIo, AtaPrimaryCtrl, AtaDriveMaster, "primary master" },
	{ AtaPrimaryIo, AtaPrimaryCtrl, AtaDriveSlave, "primary slave" },
	{ AtaSecondaryIo, AtaSecondaryCtrl, AtaDriveMaster, "secondary master" },
	{ AtaSecondaryIo, AtaSecondaryCtrl, AtaDriveSlave, "secondary slave" },
};

static uint8_t AtaSelectValue(const AtaDevice *Device, uint32_t Lba)
{
	return (uint8_t)(0xE0 | (Device->Drive << 4) | ((Lba >> 24) & 0x0F));
}

static uint8_t AtaIdentifySelectValue(const AtaProbeTarget *Target)
{
	return (uint8_t)(0xA0 | (Target->Drive << 4));
}

static void AtaDelay400ns(uint16_t CtrlBase)
{
	// ish 400ns i think
	(void)PortIORead8(CtrlBase);
	(void)PortIORead8(CtrlBase);
	(void)PortIORead8(CtrlBase);
	(void)PortIORead8(CtrlBase);
}

static bool AtaWaitNotBusy(uint16_t IoBase, uint32_t Timeout)
{
	while (Timeout-- > 0) {
		uint8_t Status = PortIORead8(IoBase + AtaRegStatus);
		if ((Status & AtaStatusBsy) == 0) {
			return true;
		}
		if (PitIsInitialized()) {
			PitDelayUs(1);
		} else {
			PortIOWait();
		}
	}
	return false;
}

static bool AtaWaitDrqOrErr(uint16_t IoBase, uint32_t Timeout)
{
	while (Timeout-- > 0) {
		uint8_t Status = PortIORead8(IoBase + AtaRegStatus);
		if ((Status & AtaStatusErr) != 0 || (Status & AtaStatusDf) != 0) {
			return false;
		}
		if ((Status & AtaStatusDrq) != 0) {
			return true;
		}
		if (PitIsInitialized()) {
			PitDelayUs(1);
		} else {
			PortIOWait();
		}
	}
	return false;
}

static bool AtaIdentify(const AtaProbeTarget *Target)
{
	PortIOWrite8(Target->IoBase + AtaRegHddevsel,
				 AtaIdentifySelectValue(Target));
	AtaDelay400ns(Target->CtrlBase);

	/* If status is 0xFF there is prob no data. */
	uint8_t Status = PortIORead8(Target->IoBase + AtaRegStatus);
	if (Status == 0xFF) {
		return false;
	}

	/* IDENTIFY */
	PortIOWrite8(Target->IoBase + AtaRegSecCount0, 0);
	PortIOWrite8(Target->IoBase + AtaRegLba0, 0);
	PortIOWrite8(Target->IoBase + AtaRegLba1, 0);
	PortIOWrite8(Target->IoBase + AtaRegLba2, 0);
	PortIOWrite8(Target->IoBase + AtaRegCommand, AtaCmdIdentify);
	AtaDelay400ns(Target->CtrlBase);

	Status = PortIORead8(Target->IoBase + AtaRegStatus);
	if (Status == 0) {
		return false;
	}

	if (!AtaWaitNotBusy(Target->IoBase, 1000000)) {
		LogWarn("dev.ata", "%s IDENTIFY timeout waiting not-busy",
				Target->Name);
		return false;
	}

	if (!AtaWaitDrqOrErr(Target->IoBase, 1000000)) {
		return false;
	}

	/* Drain IDENTIFY data (256 words). */
	for (int i = 0; i < 256; ++i) {
		(void)PortIORead16(Target->IoBase + AtaRegData);
	}

	return true;
}

bool AtaInit(void)
{
	AtaDeviceCount = 0;

	for (size_t Index = 0;
		 Index < sizeof(AtaProbeTargets) / sizeof(AtaProbeTargets[0]);
		 ++Index) {
		const AtaProbeTarget *Target = &AtaProbeTargets[Index];

		if (!AtaIdentify(Target)) {
			continue;
		}

		if (AtaDeviceCount >= AtaMaxDevices) {
			LogWarn("dev.ata", "too many ATA devices; ignoring %s",
					Target->Name);
			continue;
		}

		AtaDevices[AtaDeviceCount++] = (AtaDevice){
			.IoBase = Target->IoBase,
			.CtrlBase = Target->CtrlBase,
			.Drive = Target->Drive,
			.Name = Target->Name,
		};

		LogOk("dev.ata", "%s online (PIO)", Target->Name);
	}

	if (AtaDeviceCount == 0) {
		LogWarn("dev.ata", "no ATA devices found");
		return false;
	}

	return true;
}

size_t AtaGetDeviceCount(void)
{
	return AtaDeviceCount;
}

const AtaDevice *AtaGetDevice(size_t Index)
{
	if (Index >= AtaDeviceCount) {
		return 0;
	}

	return &AtaDevices[Index];
}

bool AtaPioRead28(const AtaDevice *Device, uint32_t Lba, uint8_t Count,
				  void *Buffer)
{
	if (Device == 0 || Buffer == NULL || Count == 0) {
		return false;
	}

	/* 28-bit LBA limit. */
	if (Lba & 0xF0000000u) {
		LogWarn("dev.ata", "LBA out of 28-bit range: %u", (unsigned int)Lba);
		return false;
	}

	uint16_t *Out = (uint16_t *)Buffer;

	/* Select drive + top 4 bits of LBA. */
	PortIOWrite8(Device->IoBase + AtaRegHddevsel, AtaSelectValue(Device, Lba));
	AtaDelay400ns(Device->CtrlBase);

	if (!AtaWaitNotBusy(Device->IoBase, 1000000)) {
		LogWarn("dev.ata", "read timeout waiting not-busy");
		return false;
	}

	PortIOWrite8(Device->IoBase + AtaRegSecCount0, Count);
	PortIOWrite8(Device->IoBase + AtaRegLba0, (uint8_t)(Lba & 0xFF));
	PortIOWrite8(Device->IoBase + AtaRegLba1, (uint8_t)((Lba >> 8) & 0xFF));
	PortIOWrite8(Device->IoBase + AtaRegLba2, (uint8_t)((Lba >> 16) & 0xFF));
	PortIOWrite8(Device->IoBase + AtaRegCommand, AtaCmdReadSectors);
	AtaDelay400ns(Device->CtrlBase);

	for (uint8_t Sector = 0; Sector < Count; ++Sector) {
		if (!AtaWaitNotBusy(Device->IoBase, 1000000)) {
			LogWarn("dev.ata", "read timeout waiting not-busy (sector %u)",
					(unsigned int)Sector);
			return false;
		}
		if (!AtaWaitDrqOrErr(Device->IoBase, 1000000)) {
			LogWarn("dev.ata", "read failed before data (sector %u)",
					(unsigned int)Sector);
			return false;
		}

		/* 256 words = 512 bytes. */
		for (int i = 0; i < 256; ++i) {
			*Out++ = PortIORead16(Device->IoBase + AtaRegData);
		}
	}

	return true;
}
