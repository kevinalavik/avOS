#include <Device/Pit.h>
#include <Arch/Irq.h>
#include <Core/Log.h>
#include <Device/PortIO.h>

#define PitChannel0 0x40
#define PitCommand 0x43
#define PitBaseFrequency 1193182

static volatile uint64_t PitTicks;

static void PitHandler(Frame *Frame)
{
	(void)Frame;
	PitTicks++;
	// for now dont do shit
}

void PitInit(uint32_t Frequency)
{
	uint32_t divisor = PitBaseFrequency / Frequency;

	PitTicks = 0;
	IrqRegisterHandler(IrqPit, PitHandler);

	PortIOWrite8(PitCommand, 0x36);
	PortIOWrite8(PitChannel0, (uint8_t)(divisor & 0xFF));
	PortIOWrite8(PitChannel0, (uint8_t)((divisor >> 8) & 0xFF));

	LogOk("device.pit", "PIT initialized at %u Hz", Frequency);
}

uint64_t PitGetTicks(void)
{
	return PitTicks;
}
