#include <Acpi/Acpi.h>
#include <Core/Log.h>
#include <Memory/Pmm.h>

#include <stddef.h>

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

void AcpiInit(uint64_t RsdpPhys)
{
	uint64_t hhdm = PmmGetHhdmOffset();
	const AcpiRsdp *Rsdp = (const AcpiRsdp *)(RsdpPhys + hhdm);

	LogInfo("core.acpi", "RSDP revision %u", Rsdp->Revision);

	if (Rsdp->Revision >= 2 && Rsdp->XsdtAddress != 0) {
		const AcpiSdth *XsdtHdr = (const AcpiSdth *)(Rsdp->XsdtAddress + hhdm);
		uint32_t count = (XsdtHdr->Length - sizeof(AcpiSdth)) / 8;
		const uint64_t *entries = (const uint64_t *)(XsdtHdr + 1);

		LogInfo("core.acpi", "XSDT at 0x%llx (%u entries)",
				(unsigned long long)Rsdp->XsdtAddress, count);
		for (uint32_t i = 0; i < count; ++i) {
			const AcpiSdth *Tbl = (const AcpiSdth *)(entries[i] + hhdm);
			char sig[5] = { Tbl->Signature[0], Tbl->Signature[1],
							Tbl->Signature[2], Tbl->Signature[3], 0 };
			LogInfo("core.acpi", "  [%u] %s", i, sig);
		}
	} else if (Rsdp->RsdtAddress != 0) {
		const AcpiSdth *RsdtHdr =
			(const AcpiSdth *)((uint64_t)Rsdp->RsdtAddress + hhdm);
		uint32_t count = (RsdtHdr->Length - sizeof(AcpiSdth)) / 4;
		const uint32_t *entries = (const uint32_t *)(RsdtHdr + 1);

		LogInfo("core.acpi", "RSDT at 0x%x (%u entries)", Rsdp->RsdtAddress,
				count);
		for (uint32_t i = 0; i < count; ++i) {
			const AcpiSdth *Tbl =
				(const AcpiSdth *)((uint64_t)entries[i] + hhdm);
			char sig[5] = { Tbl->Signature[0], Tbl->Signature[1],
							Tbl->Signature[2], Tbl->Signature[3], 0 };
			LogInfo("core.acpi", "  [%u] %s", i, sig);
		}
	}
}
