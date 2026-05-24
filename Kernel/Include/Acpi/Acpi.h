#ifndef ACPI_ACPI_H
#define ACPI_ACPI_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OemId[6];
	char OemTableId[8];
	uint32_t OemRevision;
	uint32_t CreatorId;
	uint32_t CreatorRevision;
} AcpiSdth;

typedef struct __attribute__((packed)) {
	char Signature[8];
	uint8_t Checksum;
	char OemId[6];
	uint8_t Revision;
	uint32_t RsdtAddress;
	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t ExtendedChecksum;
	uint8_t Reserved[3];
} AcpiRsdp;

void AcpiInit(uint64_t RsdpAddress);
const void *AcpiGetTable(const char Sig[4]);

#endif
