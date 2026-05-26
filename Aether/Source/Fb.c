#include <Aether/Fb.h>
#include <System/Device.h>
#include <System/Types.h>

int FbInit(Framebuffer *Fb)
{
	FbInfo Info;

	if (DeviceControl("fb", FB_CTRL_GET_INFO, &Info) != 0)
		return -1;

	U64 VAddr;
	if (DeviceControl("fb", FB_CTRL_MAP, &VAddr) != 0)
		return -1;

	Fb->Base = (volatile U32 *)VAddr;
	Fb->Width = Info.Width;
	Fb->Height = Info.Height;
	Fb->Pitch = Info.Pitch;
	Fb->Bpp = Info.Bpp;
	Fb->BufSize = (Size)Fb->Width * Fb->Height * 4;

	return 0;
}
