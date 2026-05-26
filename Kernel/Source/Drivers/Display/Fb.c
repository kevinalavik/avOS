#include <Drivers/Display/Fb.h>
#include <Drivers/Display/FbConsole.h>
#include <Drivers/Device.h>
#include <Device/Framebuffer.h>
#include <Memory/Vmm.h>
#include <Memory/Paging.h>
#include <Core/Log.h>

#include <stdbool.h>
#include <stddef.h>

static uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
{
	return (Value + Alignment - 1ull) & ~(Alignment - 1ull);
}

typedef struct FbDeviceData {
	uint64_t PhysicalBase;
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	uint32_t Bpp;
} FbDeviceData;

static void FbCopyInfo(FbInfo *Dst)
{
	Dst->Width = FramebufferWidth();
	Dst->Height = FramebufferHeight();
	Dst->Pitch = FramebufferPitch();
	Dst->Bpp = 32;
}

static void FbBind(Device *Dev)
{
	FbDeviceData *Data = (FbDeviceData *)Dev->Data;

	Data->PhysicalBase = FramebufferPhysicalAddress();
	Data->Width = FramebufferWidth();
	Data->Height = FramebufferHeight();
	Data->Pitch = FramebufferPitch();
	Data->Bpp = 32;

	LogOk("device.fb", "framebuffer: %ux%u pitch=%u addr=0x%llx",
		  (unsigned int)Data->Width, (unsigned int)Data->Height,
		  (unsigned int)Data->Pitch, (unsigned long long)Data->PhysicalBase);
}

static int64_t FbControl(Device *Dev, uint64_t Cmd, void *Arg)
{
	FbDeviceData *Data = (FbDeviceData *)Dev->Data;

	if (!FramebufferReady())
		return -1;

	switch (Cmd) {
	case FB_CTRL_GET_INFO: {
		FbInfo *Info = (FbInfo *)Arg;
		if (Info == 0)
			return -1;
		FbCopyInfo(Info);
		return 0;
	}

	case FB_CTRL_MAP: {
		uint64_t Size = (uint64_t)Data->Pitch * Data->Height;
		Size = AlignUp(Size, 0x1000ull);

		uint64_t VirtualBase = VmmReserveRegionInRange(
			0x0000000001000000ull, 0x00006FFFFFFFFFFFull, Size,
			VmmRegionMapped | VmmRegionDevice);
		if (VirtualBase == 0)
			return -1;

		uint64_t PagingFlags = PagingFlagUser | PagingFlagWritable |
							   PagingFlagWriteThrough | PagingFlagCacheDisable;

		if (!PagingMapRange(VirtualBase, Data->PhysicalBase, Size,
							PagingFlags)) {
			VmmFreeRegion(VirtualBase);
			return -1;
		}

		LogInfo("device.fb", "mapped to userspace at 0x%llx",
				(unsigned long long)VirtualBase);
		ConsoleFbClaimed();

		*(uint64_t *)Arg = VirtualBase;
		return 0;
	}

	default:
		return -1;
	}
}

static int64_t FbWrite(Device *Dev, const void *Buf, uint64_t Size)
{
	(void)Dev;
	(void)Buf;
	(void)Size;
	return -1;
}

static int64_t FbRead(Device *Dev, void *Buf, uint64_t Size)
{
	(void)Dev;
	(void)Buf;
	(void)Size;
	return -1;
}

static FbDeviceData FbDeviceDataStorage;

Driver FbDriver = {
	.Name = "framebuffer",
	.Bind = FbBind,
	.Remove = 0,
	.Read = FbRead,
	.Write = FbWrite,
	.Control = FbControl,
};

Device FbDevice = {
	.Name = "fb",
	.Drv = 0,
	.Data = &FbDeviceDataStorage,
};
