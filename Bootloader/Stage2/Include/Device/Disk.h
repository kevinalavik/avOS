#ifndef DEV_DISK_H
#define DEV_DISK_H

#include <stdbool.h>
#include <stdint.h>

#include <Filesystem/BlockFs.h>

#define DiskSectorSize 512u

typedef BlockDevice DiskDevice;

bool DiskInit(void);
const DiskDevice *DiskGetDevice(void);
bool DiskReadSectors(uint32_t Lba, uint8_t Count, void *Buffer);

#endif
