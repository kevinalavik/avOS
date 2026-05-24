#ifndef DEVICE_DISK_H
#define DEVICE_DISK_H

#include <Filesystem/BlockFs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DiskSectorSize 512u
#define DiskMaxCount 4u

typedef struct DiskInfo {
	char VolumeId;
	const char *Name;
	const char *Description;
	const BlockDevice *Device;
	bool Boot;
} DiskInfo;

bool DiskInit(void);
size_t DiskGetCount(void);
const DiskInfo *DiskGetInfo(size_t Index);
const DiskInfo *DiskGetBootDisk(void);

#endif
