#ifndef DEVICE_ATA_H
#define DEVICE_ATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AtaDevice {
	uint16_t IoBase;
	uint16_t CtrlBase;
	uint8_t Drive;
	const char *Name;
} AtaDevice;

bool AtaInit(void);
size_t AtaGetDeviceCount(void);
const AtaDevice *AtaGetDevice(size_t Index);

/* Reads `Count` 512-byte sectors starting at `Lba` into `Buffer`. */
bool AtaPioRead28(const AtaDevice *Device, uint32_t Lba, uint8_t Count,
				  void *Buffer);

#endif
