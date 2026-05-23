#include <Filesystem/BlockFs.h>

bool BlockFsReadSectors(const BlockFs *Block, uint32_t Lba, uint8_t Count,
						void *Buffer)
{
	if (Block == 0 || Block->Device == 0 || Block->Device->ReadSectors == 0) {
		return false;
	}

	return Block->Device->ReadSectors(Lba, Count, Buffer);
}
