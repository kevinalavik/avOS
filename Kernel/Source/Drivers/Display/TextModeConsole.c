#include <Drivers/Display/TextModeConsole.h>
#include <Drivers/Device.h>
#include <Device/Serial.h>
#include <Device/TextConsole.h>

static void TextModeConsoleBind(Device *Dev)
{
	(void)Dev;
}

static int64_t TextModeConsoleWrite(Device *Dev, const void *Buf, uint64_t Size)
{
	(void)Dev;

	if (Buf == 0) {
		return -1;
	}

	const char *Chars = (const char *)Buf;
	for (uint64_t Index = 0; Index < Size; ++Index) {
		if (TextConsoleReady()) {
			TextConsolePutc(Chars[Index]);
		}
		SerialPutc(Chars[Index]);
	}

	return (int64_t)Size;
}

static int64_t TextModeConsoleRead(Device *Dev, void *Buf, uint64_t Size)
{
	(void)Dev;

	if (Buf == 0 || Size == 0) {
		return -1;
	}

	Device *Keyboard = DeviceGet("kbd");
	if (Keyboard == 0) {
		return -1;
	}

	return DeviceRead(Keyboard, Buf, Size);
}

static int64_t TextModeConsoleControl(Device *Dev, uint64_t Cmd, void *Arg)
{
	(void)Dev;
	(void)Cmd;
	(void)Arg;
	return -1;
}

Driver TextModeConsoleDriver = {
	.Name = "console",
	.Bind = TextModeConsoleBind,
	.Remove = 0,
	.Read = TextModeConsoleRead,
	.Write = TextModeConsoleWrite,
	.Control = TextModeConsoleControl,
};

Device TextModeConsoleDevice = {
	.Name = "console",
	.Drv = 0,
	.Data = 0,
};
