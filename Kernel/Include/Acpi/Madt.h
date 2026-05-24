#ifndef ACPI_MADT_H
#define ACPI_MADT_H

#include <stdint.h>

#define MadtMaxIoApics 4
#define MadtMaxIsos 16

typedef struct {
	uint32_t LocalApicAddress;
	uint32_t Flags;
	uint8_t IoApicCount;
	struct {
		uint8_t Id;
		uint32_t Address;
		uint32_t GsiBase;
	} IoApics[MadtMaxIoApics];
	uint8_t IsoCount;
	struct {
		uint8_t IrqSource;
		uint32_t Gsi;
		uint16_t Flags;
	} Isos[MadtMaxIsos];
} MadtInfo;

void MadtInit(void);
const MadtInfo *MadtGetInfo(void);

#endif
