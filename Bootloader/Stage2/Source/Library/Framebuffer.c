#include <Boot/BootInfo.h>
#include <Device/PortIO.h>
#include <Library/Framebuffer.h>
#include <Library/DebugLog.h>

#include <stdbool.h>

#define PciConfigAddressPort 0x0CF8u
#define PciConfigDataPort 0x0CFCu
#define PciEnableBit 0x80000000u
#define PciInvalidVendor 0xFFFFu
#define PciMaxBus 256u
#define PciMaxDevice 32u
#define PciMaxFunction 8u
#define PciHeaderTypeOffset 0x0Eu
#define PciClassRegOffset 0x08u
#define PciBar0Offset 0x10u
#define PciBarCount 6u
#define PciBarIoSpace 0x1u
#define PciBarMemTypeMask 0x6u
#define PciBarMemType64 0x4u
#define PciBarAddressMask 0xFFFFFFF0u
#define PciVendorBochs 0x1234u
#define PciDeviceBochsVga 0x1111u
#define PciClassDisplay 0x03u
#define BochsVbeIndexPort 0x01CEu
#define BochsVbeDataPort 0x01CFu
#define BochsVbeIndexId 0u
#define BochsVbeIndexXres 1u
#define BochsVbeIndexYres 2u
#define BochsVbeIndexBpp 3u
#define BochsVbeIndexEnable 4u
#define BochsVbeDisabled 0u
#define BochsVbeEnabled 0x0001u
#define BochsVbeLinearFramebuffer 0x0040u
#define BochsVbeMinimumId 0xB0C0u
#define VesaFramebufferAddressFallback 0xE0000000u

static uint16_t BochsVbeRead(uint16_t Index)
{
	PortIOWrite(PortIOWidth16, BochsVbeIndexPort, Index);
	return (uint16_t)PortIORead(PortIOWidth16, BochsVbeDataPort);
}

static void BochsVbeWrite(uint16_t Index, uint16_t Value)
{
	PortIOWrite(PortIOWidth16, BochsVbeIndexPort, Index);
	PortIOWrite(PortIOWidth16, BochsVbeDataPort, Value);
}

static bool EnableBochsVbeFramebuffer(void)
{
	VesaModeNumber = 0;
	VesaFramebufferAddress = 0;
	VesaWidth = 0;
	VesaHeight = 0;
	VesaPitch = 0;
	VesaBpp = 0;

	if (BochsVbeRead(BochsVbeIndexId) < BochsVbeMinimumId) {
		return false;
	}

	BochsVbeWrite(BochsVbeIndexEnable, BochsVbeDisabled);
	BochsVbeWrite(BochsVbeIndexXres, BootFramebufferWidth);
	BochsVbeWrite(BochsVbeIndexYres, BootFramebufferHeight);
	BochsVbeWrite(BochsVbeIndexBpp, BootFramebufferBpp);
	BochsVbeWrite(BochsVbeIndexEnable,
				   BochsVbeEnabled | BochsVbeLinearFramebuffer);

	VesaModeNumber = 1;
	VesaFramebufferAddress = VesaFramebufferAddressFallback;
	VesaWidth = BootFramebufferWidth;
	VesaHeight = BootFramebufferHeight;
	VesaPitch = BootFramebufferPitch;
	VesaBpp = BootFramebufferBpp;

	return true;
}

static uint32_t PciConfigAddress(uint8_t Bus, uint8_t Device, uint8_t Function,
								 uint8_t Offset)
{
	return PciEnableBit | ((uint32_t)Bus << 16) | ((uint32_t)Device << 11) |
		   ((uint32_t)Function << 8) | (Offset & 0xFCu);
}

static uint32_t PciRead32(uint8_t Bus, uint8_t Device, uint8_t Function,
						  uint8_t Offset)
{
	PortIOWrite(PortIOWidth32, PciConfigAddressPort,
				PciConfigAddress(Bus, Device, Function, Offset));
	return PortIORead(PortIOWidth32, PciConfigDataPort);
}

static uint16_t PciVendorId(uint8_t Bus, uint8_t Device, uint8_t Function)
{
	return (uint16_t)(PciRead32(Bus, Device, Function, 0x00) & 0xFFFFu);
}

static uint16_t PciDeviceId(uint8_t Bus, uint8_t Device, uint8_t Function)
{
	return (uint16_t)(PciRead32(Bus, Device, Function, 0x00) >> 16);
}

static bool PciIsMultifunction(uint8_t Bus, uint8_t Device)
{
	return (PciRead32(Bus, Device, 0, PciHeaderTypeOffset) & 0x00800000u) != 0;
}

static bool PciReadFirstMemoryBar(uint8_t Bus, uint8_t Device, uint8_t Function,
								  uint32_t *AddressOut)
{
	for (uint8_t Bar = 0; Bar < PciBarCount; ++Bar) {
		uint8_t Offset = (uint8_t)(PciBar0Offset + (Bar * 4u));
		uint32_t Raw = PciRead32(Bus, Device, Function, Offset);

		if (Raw == 0 || Raw == 0xFFFFFFFFu || (Raw & PciBarIoSpace) != 0) {
			continue;
		}

		uint32_t Address = Raw & PciBarAddressMask;
		if (Address >= 0x00100000u) {
			*AddressOut = Address;
			return true;
		}

		if ((Raw & PciBarMemTypeMask) == PciBarMemType64) {
			++Bar;
		}
	}

	return false;
}

static bool PciFindBochsVgaFramebuffer(uint32_t *AddressOut)
{
	for (uint32_t Bus = 0; Bus < PciMaxBus; ++Bus) {
		for (uint8_t Device = 0; Device < PciMaxDevice; ++Device) {
			uint8_t FunctionCount = 1;

			if (PciVendorId((uint8_t)Bus, Device, 0) == PciInvalidVendor) {
				continue;
			}

			if (PciIsMultifunction((uint8_t)Bus, Device)) {
				FunctionCount = PciMaxFunction;
			}

			for (uint8_t Function = 0; Function < FunctionCount; ++Function) {
				uint16_t Vendor = PciVendorId((uint8_t)Bus, Device, Function);
				if (Vendor == PciInvalidVendor) {
					continue;
				}

				uint16_t DeviceId = PciDeviceId((uint8_t)Bus, Device, Function);
				uint32_t ClassReg = PciRead32((uint8_t)Bus, Device, Function,
											  PciClassRegOffset);
				uint8_t BaseClass = (uint8_t)(ClassReg >> 24);

				if (Vendor != PciVendorBochs || DeviceId != PciDeviceBochsVga ||
					BaseClass != PciClassDisplay) {
					continue;
				}

				uint32_t Address;
				if (PciReadFirstMemoryBar((uint8_t)Bus, Device, Function,
										  &Address)) {
					DebugLog(
						"FB",
						"Bochs/QEMU VGA PCI %u:%u.%u BAR framebuffer 0x%08X",
						(unsigned int)Bus, (unsigned int)Device,
						(unsigned int)Function, (unsigned int)Address);
					*AddressOut = Address;
					return true;
				}
			}
		}
	}

	return false;
}

static uint32_t DetectFramebufferAddress(void)
{
	uint32_t Address;

	/* Bochs/QEMU does not guarantee that the LFB is at 0xE0000000.
	 * The actual aperture is exposed by the PCI BAR of the Bochs VGA
	 * device, so prefer that over the bootstrap fallback. */
	if (PciFindBochsVgaFramebuffer(&Address)) {
		return Address;
	}

	return VesaFramebufferAddress;
}

void FramebufferInit(BootInfo *Info)
{
	if (!EnableBochsVbeFramebuffer()) {
		DebugLog("FB", "no Bochs VBE framebuffer; display unavailable");
		return;
	}

	if (VesaModeNumber == 0 || VesaFramebufferAddress == 0) {
		DebugLog("FB", "no Bochs VBE framebuffer; display unavailable");
		return;
	}

	uint32_t FramebufferAddress = DetectFramebufferAddress();
	if (FramebufferAddress == 0) {
		DebugLog("FB", "no framebuffer address found");
		return;
	}

	Info->Framebuffer.Address = (uint64_t)FramebufferAddress;
	Info->Framebuffer.Width = VesaWidth;
	Info->Framebuffer.Height = VesaHeight;
	Info->Framebuffer.Pitch = VesaPitch;
	Info->Framebuffer.Bpp = (uint8_t)VesaBpp;

	DebugLog("FB", "framebuffer at 0x%08X  %ux%u  %ubpp  pitch=%u",
		  FramebufferAddress, VesaWidth, VesaHeight, VesaBpp, VesaPitch);
}
