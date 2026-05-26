#include <Drivers/Input/Mouse.h>
#include <Drivers/Device.h>
#include <Arch/Irq.h>
#include <Device/PortIO.h>
#include <Core/Log.h>

#include <stdbool.h>
#include <stddef.h>

#define Ps2Data 0x60
#define Ps2Stat 0x64

#define MOUSE_BUF_SIZE 16

static volatile struct {
	MousePacket Data[MOUSE_BUF_SIZE];
	volatile uint16_t Head;
	volatile uint16_t Tail;
} MouseRing;

static struct {
	uint32_t X;
	uint32_t Y;
	uint32_t MaxX;
	uint32_t MaxY;
} MouseState;

static int MousePhase;
static uint8_t MousePktBytes[3];

static void MouseWaitWrite(void)
{
	for (int i = 0; i < 10000; ++i) {
		if (!(PortIORead8(Ps2Stat) & 2))
			return;
	}
}

static void MouseWaitRead(void)
{
	for (int i = 0; i < 10000; ++i) {
		if (PortIORead8(Ps2Stat) & 1)
			return;
	}
}

static void MouseCommand(uint8_t Cmd)
{
	MouseWaitWrite();
	PortIOWrite8(Ps2Stat, 0xD4);
	MouseWaitWrite();
	PortIOWrite8(Ps2Data, Cmd);
}

static uint8_t MouseReadData(void)
{
	MouseWaitRead();
	return PortIORead8(Ps2Data);
}

static Frame *MouseIrqHandler(Frame *Frame)
{
	uint8_t Byte = PortIORead8(Ps2Data);

	switch (MousePhase) {
	case 0:
		if (!(Byte & 0x08))
			return Frame;
		MousePktBytes[0] = Byte;
		MousePhase = 1;
		break;
	case 1:
		MousePktBytes[1] = Byte;
		MousePhase = 2;
		break;
	case 2: {
		MousePktBytes[2] = Byte;
		MousePhase = 0;

		int8_t Dx8 = (int8_t)MousePktBytes[1];
		int8_t Dy8 = (int8_t)MousePktBytes[2];
		uint8_t Btn = MousePktBytes[0] & 0x07;

		int32_t NewX = (int32_t)MouseState.X + (int32_t)Dx8;
		int32_t NewY = (int32_t)MouseState.Y - (int32_t)Dy8;

		if (NewX < 0)
			NewX = 0;
		if (NewX > (int32_t)MouseState.MaxX)
			NewX = (int32_t)MouseState.MaxX;
		if (NewY < 0)
			NewY = 0;
		if (NewY > (int32_t)MouseState.MaxY)
			NewY = (int32_t)MouseState.MaxY;

		MouseState.X = (uint32_t)NewX;
		MouseState.Y = (uint32_t)NewY;

		uint16_t Nxt = (MouseRing.Head + 1) & (MOUSE_BUF_SIZE - 1);
		if (Nxt != MouseRing.Tail) {
			MouseRing.Data[MouseRing.Head].X = MouseState.X;
			MouseRing.Data[MouseRing.Head].Y = MouseState.Y;
			MouseRing.Data[MouseRing.Head].Dx = (int32_t)Dx8;
			MouseRing.Data[MouseRing.Head].Dy = (int32_t)Dy8;
			MouseRing.Data[MouseRing.Head].Buttons = Btn;
			MouseRing.Head = Nxt;
		}
		break;
	}
	}

	return Frame;
}

static void MouseBind(Device *Dev)
{
	(void)Dev;

	MousePhase = 0;
	MouseRing.Head = 0;
	MouseRing.Tail = 0;
	MouseState.X = 0;
	MouseState.Y = 0;
	MouseState.MaxX = 1023;
	MouseState.MaxY = 767;

	MouseWaitWrite();
	PortIOWrite8(Ps2Stat, 0xA8);

	MouseWaitWrite();
	PortIOWrite8(Ps2Stat, 0x20);
	uint8_t Cfg = MouseReadData();
	Cfg |= 0x22;
	MouseWaitWrite();
	PortIOWrite8(Ps2Stat, 0x60);
	MouseWaitWrite();
	PortIOWrite8(Ps2Data, Cfg);

	MouseCommand(0xF6);
	MouseReadData();
	MouseCommand(0xF4);
	MouseReadData();

	IrqRegisterHandler(IrqMouse, MouseIrqHandler);

	LogOk("device.mouse", "PS/2 mouse online");
}

static int64_t MouseRead(Device *Dev, void *Buf, uint64_t Size)
{
	(void)Dev;
	if (Size < sizeof(MousePacket))
		return -1;

	uint16_t Tail = MouseRing.Tail;
	if (Tail == MouseRing.Head)
		return 0;

	MousePacket *Pkt = (MousePacket *)Buf;
	*Pkt = MouseRing.Data[Tail];
	MouseRing.Tail = (Tail + 1) & (MOUSE_BUF_SIZE - 1);

	return (int64_t)sizeof(MousePacket);
}

static int64_t MouseWrite(Device *Dev, const void *Buf, uint64_t Size)
{
	(void)Dev;
	(void)Buf;
	(void)Size;
	return -1;
}

static int64_t MouseControl(Device *Dev, uint64_t Cmd, void *Arg)
{
	(void)Dev;
	switch (Cmd) {
	case MOUSE_CTRL_SET_BOUNDS: {
		uint32_t *Bounds = (uint32_t *)Arg;
		MouseState.MaxX = Bounds[0];
		MouseState.MaxY = Bounds[1];
		return 0;
	}
	case MOUSE_CTRL_SET_POS: {
		uint32_t *Pos = (uint32_t *)Arg;
		MouseState.X = Pos[0];
		MouseState.Y = Pos[1];
		return 0;
	}
	case MOUSE_CTRL_GET_COUNT:
		return (int64_t)((MouseRing.Head - MouseRing.Tail) &
						 (MOUSE_BUF_SIZE - 1));
	default:
		return -1;
	}
}

Driver MouseDriver = {
	.Name = "ps2-mouse",
	.Bind = MouseBind,
	.Remove = 0,
	.Read = MouseRead,
	.Write = MouseWrite,
	.Control = MouseControl,
};

Device MouseDevice = {
	.Name = "mouse",
	.Drv = 0,
	.Data = 0,
};
