#include <Device/Pit.h>
#include <Arch/Irq.h>
#include <Core/Log.h>
#include <Device/PortIO.h>

#define PitChannel0 0x40
#define PitCommand 0x43
#define PitBaseFrequency 1193182

static volatile uint64_t PitTicks;
static uint32_t PitFrequencyHz;
static uint16_t PitDivisor;
static int PitInitialized;

static uint16_t PitReadCounter0(void)
{
	PortIOWrite8(PitCommand, 0x00);
	uint8_t lo = PortIORead8(PitChannel0);
	uint8_t hi = PortIORead8(PitChannel0);
	return (uint16_t)lo | ((uint16_t)hi << 8);
}

static void PitWaitBaseCycles(uint64_t Cycles)
{
	uint16_t prev = PitReadCounter0();
	uint64_t elapsed = 0;

	while (elapsed < Cycles) {
		uint16_t now = PitReadCounter0();
		uint16_t delta;
		if (now <= prev) {
			delta = (uint16_t)(prev - now);
		} else {
			delta = (uint16_t)(prev + (uint16_t)(PitDivisor - now));
		}
		elapsed += delta;
		prev = now;
	}
}

static void PitHandler(Frame *Frame)
{
	(void)Frame;
	PitTicks++;
	// for now dont do shit
}

void PitInit(uint32_t Frequency)
{
	if (Frequency == 0) {
		Frequency = 100;
	}
	uint32_t divisor32 = PitBaseFrequency / Frequency;
	if (divisor32 == 0) {
		divisor32 = 1;
	}
	if (divisor32 > 0xFFFFu) {
		divisor32 = 0xFFFFu;
	}
	uint16_t divisor = (uint16_t)divisor32;

	PitTicks = 0;
	PitFrequencyHz = Frequency;
	PitDivisor = divisor;
	PitInitialized = 1;
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

void PitDelayUs(uint64_t Microseconds)
{
	if (!PitInitialized || Microseconds == 0) {
		return;
	}
	uint64_t cycles = (PitBaseFrequency * Microseconds) / 1000000ull;
	if (cycles == 0) {
		cycles = 1;
	}
	PitWaitBaseCycles(cycles);
}

void PitDelayMs(uint64_t Milliseconds)
{
	PitDelayUs(Milliseconds * 1000ull);
}

uint32_t PitGetFrequencyHz(void)
{
	return PitFrequencyHz;
}

int PitIsInitialized(void)
{
	return PitInitialized;
}
