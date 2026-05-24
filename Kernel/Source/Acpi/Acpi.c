#include <Acpi/Acpi.h>
#include <Core/Log.h>
#include <Memory/Pmm.h>

#include <stddef.h>

static const AcpiSdth *AcpiSdt;
static bool AcpiIsXsdt;

void AcpiInit(uint64_t RsdpPhys)
{
	const AcpiRsdp *Rsdp = (const AcpiRsdp *)(RsdpPhys + PmmGetHhdmOffset());

	LogInfo("core.acpi", "RSDP revision %u", Rsdp->Revision);

	if (Rsdp->Revision >= 2 && Rsdp->XsdtAddress != 0) {
		AcpiSdt = (const AcpiSdth *)(Rsdp->XsdtAddress + PmmGetHhdmOffset());
		AcpiIsXsdt = true;
		uint32_t n = (AcpiSdt->Length - sizeof(AcpiSdth)) / 8;
		const uint64_t *e = (const uint64_t *)(AcpiSdt + 1);
		LogInfo("core.acpi", "XSDT at 0x%llx  %u tables",
				(unsigned long long)Rsdp->XsdtAddress, n);
		for (uint32_t i = 0; i < n; ++i) {
			const AcpiSdth *t = (const AcpiSdth *)(e[i] + PmmGetHhdmOffset());
			LogInfo("core.acpi", "  [%02u] %.4s", i, t->Signature);
		}
	} else if (Rsdp->RsdtAddress != 0) {
		AcpiSdt = (const AcpiSdth *)((uint64_t)Rsdp->RsdtAddress +
									 PmmGetHhdmOffset());
		AcpiIsXsdt = false;
		uint32_t n = (AcpiSdt->Length - sizeof(AcpiSdth)) / 4;
		const uint32_t *e = (const uint32_t *)(AcpiSdt + 1);
		LogInfo("core.acpi", "RSDT at 0x%x  %u tables", Rsdp->RsdtAddress, n);
		for (uint32_t i = 0; i < n; ++i) {
			const AcpiSdth *t =
				(const AcpiSdth *)((uint64_t)e[i] + PmmGetHhdmOffset());
			LogInfo("core.acpi", "  [%02u] %.4s", i, t->Signature);
		}
	} else {
		LogWarn("core.acpi", "no SDT found");
		AcpiSdt = 0;
	}
}

const void *AcpiGetTable(const char Sig[4])
{
	if (AcpiSdt == 0)
		return 0;

	uint32_t count;
	if (AcpiIsXsdt) {
		count = (AcpiSdt->Length - sizeof(AcpiSdth)) / 8;
		const uint64_t *ents = (const uint64_t *)(AcpiSdt + 1);
		for (uint32_t i = 0; i < count; ++i) {
			const AcpiSdth *tbl =
				(const AcpiSdth *)(ents[i] + PmmGetHhdmOffset());
			if (tbl->Signature[0] == Sig[0] && tbl->Signature[1] == Sig[1] &&
				tbl->Signature[2] == Sig[2] && tbl->Signature[3] == Sig[3])
				return tbl;
		}
	} else {
		count = (AcpiSdt->Length - sizeof(AcpiSdth)) / 4;
		const uint32_t *ents = (const uint32_t *)(AcpiSdt + 1);
		for (uint32_t i = 0; i < count; ++i) {
			const AcpiSdth *tbl =
				(const AcpiSdth *)((uint64_t)ents[i] + PmmGetHhdmOffset());
			if (tbl->Signature[0] == Sig[0] && tbl->Signature[1] == Sig[1] &&
				tbl->Signature[2] == Sig[2] && tbl->Signature[3] == Sig[3])
				return tbl;
		}
	}

	return 0;
}
