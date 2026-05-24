#include <Arch/Irq.h>
#include <Acpi/Madt.h>
#include <Core/Log.h>
#include <Device/PortIO.h>
#include <Memory/Pmm.h>

#include <stddef.h>

#define IrqCount 16
#define IrqMaxIoApics 4

#define PicMasterCmd 0x20
#define PicMasterData 0x21
#define PicSlaveCmd 0xA0
#define PicSlaveData 0xA1

#define PicIrqBase 0x20
#define PicIcw1Icw4 0x11
#define PicIcw4_8086 0x01

#define LapicId 0x020
#define LapicTpr 0x080
#define LapicEoi 0x0B0
#define LapicLdr 0x0D0
#define LapicDfr 0x0E0
#define LapicSvr 0x0F0
#define LapicLvt0 0x350
#define LapicLvt1 0x360
#define LapicLvterr 0x370

#define IoApicId 0x00
#define IoApicVer 0x01
#define IoApicRedirLo(i) (0x10 + (i) * 2)
#define IoApicRedirHi(i) (0x10 + (i) * 2 + 1)

static IrqHandler IrqHandlers[IrqCount];

static bool UseApic;
static uint64_t LapicVirt;
static uint8_t LapicBspId;
static uint8_t IrqIoApicCount;
static struct {
	uint64_t VirtBase;
	uint32_t GsiBase;
	uint8_t MaxEntry;
} IrqIoApics[IrqMaxIoApics];

static uint32_t LapicRead(uint32_t Off)
{
	return *(volatile uint32_t *)(LapicVirt + Off);
}

static void LapicWrite(uint32_t Off, uint32_t Val)
{
	*(volatile uint32_t *)(LapicVirt + Off) = Val;
}

static uint32_t IoApicRead(uint64_t Base, uint32_t Reg)
{
	*(volatile uint32_t *)Base = Reg;
	return *(volatile uint32_t *)(Base + 0x10);
}

static void IoApicWrite(uint64_t Base, uint32_t Reg, uint32_t Val)
{
	*(volatile uint32_t *)Base = Reg;
	*(volatile uint32_t *)(Base + 0x10) = Val;
	*(volatile uint32_t *)Base;
}

static void IrqHandlerDefault(Frame *Frame)
{
	(void)Frame;
	LogWarn("core.arch.irq", "unhandled IRQ %llu",
			(unsigned long long)(Frame->vector - PicIrqBase));
}

static void IrqInitPic(void)
{
	for (size_t i = 0; i < IrqCount; ++i) {
		IrqHandlers[i] = IrqHandlerDefault;
	}

	PortIOWrite8(PicMasterCmd, PicIcw1Icw4);
	PortIOWrite8(PicSlaveCmd, PicIcw1Icw4);

	PortIOWrite8(PicMasterData, 0x20);
	PortIOWrite8(PicSlaveData, 0x28);

	PortIOWrite8(PicMasterData, 0x04);
	PortIOWrite8(PicSlaveData, 0x02);

	PortIOWrite8(PicMasterData, PicIcw4_8086);
	PortIOWrite8(PicSlaveData, PicIcw4_8086);

	PortIOWrite8(PicMasterData, 0xFF);
	PortIOWrite8(PicSlaveData, 0xFF);

	LogInfo("core.arch.irq", "PIC remapped (IRQ0-15 -> vectors 32-47)");
}

static void IrqMaskPic(void)
{
	PortIOWrite8(PicMasterData, 0xFF);
	PortIOWrite8(PicSlaveData, 0xFF);
}

static void IrqUnmaskPic(uint8_t Irq)
{
	static uint8_t masterMask = 0xFF;
	static uint8_t slaveMask = 0xFF;

	if (Irq >= IrqCount)
		return;

	if (Irq < 8) {
		masterMask &= ~(uint8_t)(1u << Irq);
		PortIOWrite8(PicMasterData, masterMask);
	} else {
		slaveMask &= ~(uint8_t)(1u << (Irq - 8));
		PortIOWrite8(PicMasterData, masterMask);
		PortIOWrite8(PicSlaveData, slaveMask);
	}
}

static bool IrqInitApic(void)
{
	const MadtInfo *Madt = MadtGetInfo();

	if (Madt->LocalApicAddress == 0) {
		return false;
	}

	LapicVirt = PmmPhysToHhdm(Madt->LocalApicAddress);
	if (LapicVirt == 0) {
		LogError("core.arch.irq", "failed to map local APIC at 0x%x",
				 Madt->LocalApicAddress);
		return false;
	}

	LapicBspId = (uint8_t)(LapicRead(LapicId) >> 24u);
	uint32_t svr = LapicRead(LapicSvr);
	svr = (svr & ~0x1FFu) | (1u << 8) | 0xFF;
	LapicWrite(LapicSvr, svr);
	LapicWrite(LapicDfr, 0xFFFFFFFF);
	LapicWrite(LapicLdr, 0);
	LapicWrite(LapicTpr, 0);
	LapicWrite(LapicLvt0, 1u << 16);
	LapicWrite(LapicLvt1, 1u << 16);
	LapicWrite(LapicLvterr, 1u << 16);
	LapicWrite(LapicEoi, 0);

	IrqIoApicCount = 0;
	for (uint8_t i = 0; i < Madt->IoApicCount && IrqIoApicCount < IrqMaxIoApics;
		 ++i) {
		uint64_t base = PmmPhysToHhdm(Madt->IoApics[i].Address);
		if (base == 0) {
			LogWarn("core.arch.irq", "failed to map IOAPIC #%u at 0x%x",
					Madt->IoApics[i].Id, Madt->IoApics[i].Address);
			continue;
		}

		uint32_t ver = IoApicRead(base, IoApicVer);
		uint8_t maxEntry = (uint8_t)((ver >> 16) & 0xFF);

		IrqIoApics[IrqIoApicCount].VirtBase = base;
		IrqIoApics[IrqIoApicCount].GsiBase = Madt->IoApics[i].GsiBase;
		IrqIoApics[IrqIoApicCount].MaxEntry = maxEntry;
		IrqIoApicCount++;

		for (uint8_t e = 0; e <= maxEntry; ++e) {
			IoApicWrite(base, IoApicRedirLo(e), 1u << 16);
			IoApicWrite(base, IoApicRedirHi(e), 0);
		}

		LogInfo("core.arch.irq",
				"IOAPIC ID=%u at 0x%llx GSI base=%u max entry=%u",
				Madt->IoApics[i].Id, (unsigned long long)base,
				Madt->IoApics[i].GsiBase, maxEntry);
	}

	IrqMaskPic();

	LogOk("core.arch.irq", "APIC initialized (LAPIC ID=%u, %u IOAPICs)",
		  LapicBspId, IrqIoApicCount);
	return true;
}

void IrqInit(void)
{
	for (size_t i = 0; i < IrqCount; ++i) {
		IrqHandlers[i] = IrqHandlerDefault;
	}

	UseApic = false;

	if (IrqInitApic()) {
		UseApic = true;
	} else {
		LogWarn("core.arch.irq", "APIC unavailable, falling back to PIC");
		IrqInitPic();
	}
}

static bool IrqFindGsi(uint8_t Irq, uint32_t *GsiOut, uint16_t *FlagsOut)
{
	const MadtInfo *Madt = MadtGetInfo();
	for (uint8_t i = 0; i < Madt->IsoCount; ++i) {
		if (Madt->Isos[i].IrqSource == Irq) {
			*GsiOut = Madt->Isos[i].Gsi;
			*FlagsOut = Madt->Isos[i].Flags;
			return true;
		}
	}

	*GsiOut = Irq;
	*FlagsOut = 0;
	return true;
}

static bool IrqFindIoApic(uint32_t Gsi, uint64_t *BaseOut, uint8_t *PinOut)
{
	for (uint8_t i = 0; i < IrqIoApicCount; ++i) {
		uint32_t end = IrqIoApics[i].GsiBase + IrqIoApics[i].MaxEntry;
		if (Gsi >= IrqIoApics[i].GsiBase && Gsi < end) {
			*BaseOut = IrqIoApics[i].VirtBase;
			*PinOut = (uint8_t)(Gsi - IrqIoApics[i].GsiBase);
			return true;
		}
	}
	return false;
}

static bool IrqRouteApic(uint8_t Irq, uint8_t Vector)
{
	uint32_t gsi;
	uint16_t flags;

	if (!IrqFindGsi(Irq, &gsi, &flags)) {
		return false;
	}

	uint64_t base;
	uint8_t pin;
	if (!IrqFindIoApic(gsi, &base, &pin)) {
		return false;
	}

	uint8_t polarity = (uint8_t)(flags & 3u);
	uint8_t trigger = (uint8_t)((flags >> 2) & 3u);

	bool activeLow = (polarity == 3);
	bool levelTrig = (trigger == 3);

	uint32_t low = Vector;
	low |= (uint32_t)(0u << 8); /* fixed deliviry mode */
	low |= (uint32_t)(0u << 11);
	if (activeLow)
		low |= 1u << 13;
	if (levelTrig)
		low |= 1u << 15;
	/* bit 16 = 0 (unmasked) */

	uint32_t high = (uint32_t)LapicBspId << 24;

	IoApicWrite(base, IoApicRedirLo(pin), low);
	IoApicWrite(base, IoApicRedirHi(pin), high);

	LogDebug("core.arch.irq",
			 "IRQ%u -> GSI=%u IOAPIC pin=%u vector=%u flags=0x%x", Irq, gsi,
			 pin, Vector, flags);
	return true;
}

void IrqRegisterHandler(uint8_t Irq, IrqHandler Handler)
{
	if (Irq >= IrqCount)
		return;

	IrqHandlers[Irq] = Handler;

	if (UseApic) {
		if (!IrqRouteApic(Irq, PicIrqBase + Irq)) {
			LogWarn("core.arch.irq", "failed to route IRQ%u through IOAPIC",
					Irq);
		}
	} else {
		IrqUnmaskPic(Irq);
	}
}

void IrqDispatch(Frame *Frame)
{
	uint8_t irq = Frame->vector - PicIrqBase;

	if (irq < IrqCount) {
		IrqHandlers[irq](Frame);
	}

	if (UseApic) {
		LapicWrite(LapicEoi, 0);
	} else {
		if (irq >= 8) {
			PortIOWrite8(PicSlaveCmd, 0x20);
		}
		PortIOWrite8(PicMasterCmd, 0x20);
	}
}
