#ifndef ARCHITECTURE_ACPI_H
#define ARCHITECTURE_ACPI_H

#include <stdint.h>

typedef struct AcpiRootPointers {
	uint64_t RsdpAddress;
} AcpiRootPointers;

AcpiRootPointers AcpiFindRootPointers(void);

#endif
