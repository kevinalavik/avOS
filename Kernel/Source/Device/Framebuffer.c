#include <Device/Framebuffer.h>

static volatile uint32_t *FbBase;
static uint32_t FbWidth;
static uint32_t FbHeight;
static uint32_t FbStride;

bool FramebufferInit(const BootFramebuffer *Fb)
{
	if (!Fb || Fb->Address == 0 || Fb->Width == 0 || Fb->Height == 0 ||
		Fb->Pitch == 0 || Fb->Bpp != 32) {
		return false;
	}

	FbBase = (volatile uint32_t *)(uintptr_t)Fb->Address;
	FbWidth = Fb->Width;
	FbHeight = Fb->Height;
	FbStride = Fb->Pitch / 4u;

	return true;
}

bool FramebufferReady(void)
{
	return FbBase != 0;
}

uint32_t FramebufferWidth(void)
{
	return FbWidth;
}

uint32_t FramebufferHeight(void)
{
	return FbHeight;
}

uint32_t FramebufferPitch(void)
{
	return FbStride * 4u;
}

uint64_t FramebufferPhysicalAddress(void)
{
	return (uint64_t)(uintptr_t)FbBase;
}

uint32_t FramebufferColor(uint8_t R, uint8_t G, uint8_t B)
{
	return (uint32_t)B | ((uint32_t)G << 8) | ((uint32_t)R << 16);
}

void FramebufferPutPixel(uint32_t X, uint32_t Y, uint32_t Color)
{
	if (!FbBase || X >= FbWidth || Y >= FbHeight) {
		return;
	}
	FbBase[Y * FbStride + X] = Color;
}

void FramebufferClear(uint32_t Color)
{
	if (!FbBase) {
		return;
	}

	for (uint32_t Y = 0; Y < FbHeight; ++Y) {
		volatile uint32_t *Row = FbBase + Y * FbStride;
		for (uint32_t X = 0; X < FbWidth; ++X) {
			Row[X] = Color;
		}
	}
}