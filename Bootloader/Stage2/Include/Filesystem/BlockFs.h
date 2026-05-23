#ifndef FS_BLOCKFS_H
#define FS_BLOCKFS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct BlockDevice {
	bool (*ReadSectors)(uint32_t Lba, uint8_t Count, void *Buffer);
} BlockDevice;

typedef struct BlockFs {
	const BlockDevice *Device;
} BlockFs;

bool BlockFsReadSectors(const BlockFs *Block, uint32_t Lba, uint8_t Count,
						void *Buffer);

#endif
