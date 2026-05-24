#include <Acpi/Madt.h>
#include <Acpi/Acpi.h>
#include <Core/Log.h>

#include <stddef.h>

typedef struct __attribute__((packed)) {
	AcpiSdth Header;
	uint32_t LocalApicAddress;
	uint32_t Flags;
} AcpiMadt;

typedef struct __attribute__((packed)) {
	uint8_t Type;
	uint8_t Length;
} AcpiMadtEntryHeader;

#define AcpiMadtEntryTypeProcessorLocalApic 0
#define AcpiMadtEntryTypeIoApic 1
#define AcpiMadtEntryTypeInterruptSourceOverride 2

typedef struct __attribute__((packed)) {
	uint8_t Type;
	uint8_t Length;
	uint8_t AcpiProcessorId;
	uint8_t ApicId;
	uint32_t Flags;
} AcpiMadtProcessorLocalApic;

typedef struct __attribute__((packed)) {
	uint8_t Type;
	uint8_t Length;
	uint8_t IoApicId;
	uint8_t Reserved;
	uint32_t IoApicAddress;
	uint32_t GlobalSystemInterruptBase;
} AcpiMadtIoApic;

typedef struct __attribute__((packed)) {
	uint8_t Type;
	uint8_t Length;
	uint8_t BusSource;
	uint8_t IrqSource;
	uint32_t GlobalSystemInterrupt;
	uint16_t Flags;
} AcpiMadtInterruptSourceOverride;

static MadtInfo ParsedMadt;

void MadtInit(void)
{
	const AcpiMadt *Madt = (const AcpiMadt *)AcpiGetTable("APIC");

	if (Madt == 0) {
		LogWarn("core.acpi.madt", "MADT table not found");
		return;
	}

	ParsedMadt.LocalApicAddress = Madt->LocalApicAddress;
	ParsedMadt.Flags = Madt->Flags;
	ParsedMadt.IoApicCount = 0;
	ParsedMadt.IsoCount = 0;

	LogInfo("core.acpi.madt", "local APIC at 0x%x, flags=0x%x",
			Madt->LocalApicAddress, Madt->Flags);

	uintptr_t addr = (uintptr_t)Madt;
	uint32_t offset = sizeof(AcpiMadt);
	uint32_t count = 0;

	while (offset < Madt->Header.Length) {
		const AcpiMadtEntryHeader *entry =
			(const AcpiMadtEntryHeader *)(addr + offset);

		switch (entry->Type) {
		case AcpiMadtEntryTypeProcessorLocalApic: {
			const AcpiMadtProcessorLocalApic *proc =
				(const AcpiMadtProcessorLocalApic *)entry;
			LogDebug("core.acpi.madt",
					 " processor: ACPI ID=%u APIC ID=%u flags=0x%x",
					 proc->AcpiProcessorId, proc->ApicId, proc->Flags);
			break;
		}
		case AcpiMadtEntryTypeIoApic: {
			if (ParsedMadt.IoApicCount < MadtMaxIoApics) {
				const AcpiMadtIoApic *ioapic = (const AcpiMadtIoApic *)entry;
				uint8_t idx = ParsedMadt.IoApicCount++;
				ParsedMadt.IoApics[idx].Id = ioapic->IoApicId;
				ParsedMadt.IoApics[idx].Address = ioapic->IoApicAddress;
				ParsedMadt.IoApics[idx].GsiBase =
					ioapic->GlobalSystemInterruptBase;
				LogInfo("core.acpi.madt",
						"  IOAPIC: ID=%u addr=0x%x GSI base=%u",
						ioapic->IoApicId, ioapic->IoApicAddress,
						ioapic->GlobalSystemInterruptBase);
			} else {
				LogWarn("core.acpi.madt", "too many IOAPICs, ignoring");
			}
			break;
		}
		case AcpiMadtEntryTypeInterruptSourceOverride: {
			if (ParsedMadt.IsoCount < MadtMaxIsos) {
				const AcpiMadtInterruptSourceOverride *iso =
					(const AcpiMadtInterruptSourceOverride *)entry;
				uint8_t idx = ParsedMadt.IsoCount++;
				ParsedMadt.Isos[idx].IrqSource = iso->IrqSource;
				ParsedMadt.Isos[idx].Gsi = iso->GlobalSystemInterrupt;
				ParsedMadt.Isos[idx].Flags = iso->Flags;
				LogInfo("core.acpi.madt",
						"  ISO: bus=0x%x IRQ=%u -> GSI=%u flags=0x%x",
						iso->BusSource, iso->IrqSource,
						iso->GlobalSystemInterrupt, iso->Flags);
			} else {
				LogWarn("core.acpi.madt", "too many ISOs, ignoring");
			}
			break;
		}
		default:
			LogTrace("core.acpi.madt", " entry type=%u len=%u", entry->Type,
					 entry->Length);
			break;
		}

		offset += entry->Length;
		++count;
	}

	LogInfo("core.acpi.madt", "%u entries parsed", count);
}

const MadtInfo *MadtGetInfo(void)
{
	return &ParsedMadt;
}
