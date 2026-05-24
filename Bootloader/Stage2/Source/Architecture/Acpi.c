#include <Architecture/Acpi.h>

#include <Library/DebugLog.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AcpiRsdpSignatureLength 8u
#define AcpiRsdpV1Length 20u
#define AcpiRsdpV2MinimumLength 36u
#define AcpiEbdaSegmentPointer 0x40e
#define AcpiEbdaSearchLength 1024u
#define AcpiBiosSearchBase 0x000e0000u
#define AcpiBiosSearchEnd 0x00100000u
#define AcpiRsdpAlignment 16u

typedef struct AcpiRsdp {
	char Signature[8];
	uint8_t Checksum;
	char OemId[6];
	uint8_t Revision;
	uint32_t RsdtAddress;
	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t ExtendedChecksum;
	uint8_t Reserved[3];
} __attribute__((packed)) AcpiRsdp;

static bool SignatureMatches(const AcpiRsdp *Rsdp)
{
	static const char Signature[AcpiRsdpSignatureLength] = {
		'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '
	};

	for (size_t Index = 0; Index < AcpiRsdpSignatureLength; ++Index) {
		if (Rsdp->Signature[Index] != Signature[Index]) {
			return false;
		}
	}

	return true;
}

static bool ChecksumValid(const void *Data, uint32_t Length)
{
	const uint8_t *Bytes = (const uint8_t *)Data;
	uint8_t Sum = 0;

	for (uint32_t Index = 0; Index < Length; ++Index) {
		Sum = (uint8_t)(Sum + Bytes[Index]);
	}

	return Sum == 0;
}

static bool RsdpValid(const AcpiRsdp *Rsdp, uint32_t AvailableLength)
{
	if (!SignatureMatches(Rsdp) ||
		!ChecksumValid(Rsdp, AcpiRsdpV1Length)) {
		return false;
	}

	if (Rsdp->Revision == 0) {
		return true;
	}

	if (Rsdp->Length < AcpiRsdpV2MinimumLength ||
		Rsdp->Length > AvailableLength) {
		return false;
	}

	return ChecksumValid(Rsdp, Rsdp->Length);
}

static const AcpiRsdp *SearchRange(uintptr_t Base, uintptr_t Length)
{
	uintptr_t End = Base + Length;

	for (uintptr_t Address = Base; Address + AcpiRsdpV1Length <= End;
		 Address += AcpiRsdpAlignment) {
		const AcpiRsdp *Rsdp = (const AcpiRsdp *)Address;

		if (RsdpValid(Rsdp, (uint32_t)(End - Address))) {
			return Rsdp;
		}
	}

	return 0;
}

static const AcpiRsdp *SearchEbda(void)
{
	const uint16_t *SegmentPointer = (const uint16_t *)AcpiEbdaSegmentPointer;
	uintptr_t Base = ((uintptr_t)*SegmentPointer) << 4;

	if (Base == 0) {
		return 0;
	}

	return SearchRange(Base, AcpiEbdaSearchLength);
}

AcpiRootPointers AcpiFindRootPointers(void)
{
	AcpiRootPointers Roots = { 0, 0, 0, 0 };
	const AcpiRsdp *Rsdp = SearchEbda();

	if (Rsdp == 0) {
		Rsdp = SearchRange(AcpiBiosSearchBase,
						   AcpiBiosSearchEnd - AcpiBiosSearchBase);
	}

	if (Rsdp == 0) {
		DebugLog("ACPI", "RSDP not found");
		return Roots;
	}

	Roots.RsdpAddress = (uint64_t)(uintptr_t)Rsdp;
	Roots.RsdtAddress = Rsdp->RsdtAddress;
	Roots.Revision = Rsdp->Revision;
	if (Rsdp->Revision != 0 && Rsdp->Length >= AcpiRsdpV2MinimumLength) {
		Roots.XsdtAddress = Rsdp->XsdtAddress;
	}

	DebugLog("ACPI", "RSDP 0x%08x revision %u",
		  (unsigned int)Roots.RsdpAddress, (unsigned int)Roots.Revision);
	DebugLog("ACPI", "RSDT 0x%08x XSDT 0x%08x%08x",
			(unsigned int)Roots.RsdtAddress,
			(unsigned int)(Roots.XsdtAddress >> 32),
			(unsigned int)Roots.XsdtAddress);

	return Roots;
}
