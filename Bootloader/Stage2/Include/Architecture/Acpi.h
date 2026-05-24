#ifndef ARCHITECTURE_ACPI_H
#define ARCHITECTURE_ACPI_H

#include <stdint.h>

typedef struct AcpiRootPointers {
	uint64_t RsdpAddress;
	uint32_t RsdtAddress;
	uint64_t XsdtAddress;
	uint8_t Revision;
} AcpiRootPointers;

AcpiRootPointers AcpiFindRootPointers(void);

#endif
