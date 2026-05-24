#include <Device/Keyboard.h>
#include <Device/Device.h>
#include <Arch/Irq.h>
#include <Core/Log.h>
#include <Device/PortIO.h>
#include <Library/Stdout.h>

#include <stdbool.h>

#define KbdData 0x60
#define KbdStat 0x64
#define KbdBufSize 256

static volatile struct {
	char Data[KbdBufSize];
	volatile uint16_t Head;
	volatile uint16_t Tail;
} KbdBuffer;

static struct {
	bool Shift;
	bool Ctrl;
	bool Alt;
	bool Caps;
} KbdMod;

static bool KbdEcho;

static const char KbdNorm[128] = {
	[0x01] = 0x1B, [0x02] = '1', [0x03] = '2',	[0x04] = '3',  [0x05] = '4',
	[0x06] = '5',  [0x07] = '6', [0x08] = '7',	[0x09] = '8',  [0x0A] = '9',
	[0x0B] = '0',  [0x0C] = '-', [0x0D] = '=',	[0x0E] = '\b', [0x0F] = '\t',
	[0x10] = 'q',  [0x11] = 'w', [0x12] = 'e',	[0x13] = 'r',  [0x14] = 't',
	[0x15] = 'y',  [0x16] = 'u', [0x17] = 'i',	[0x18] = 'o',  [0x19] = 'p',
	[0x1A] = '[',  [0x1B] = ']', [0x1C] = '\n', [0x1E] = 'a',  [0x1F] = 's',
	[0x20] = 'd',  [0x21] = 'f', [0x22] = 'g',	[0x23] = 'h',  [0x24] = 'j',
	[0x25] = 'k',  [0x26] = 'l', [0x27] = ';',	[0x28] = '\'', [0x29] = '`',
	[0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x',	[0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b',  [0x31] = 'n', [0x32] = 'm',	[0x33] = ',',  [0x34] = '.',
	[0x35] = '/',  [0x39] = ' ',
};

static const char KbdShft[128] = {
	[0x01] = 0x1B, [0x02] = '!', [0x03] = '@',	[0x04] = '#',  [0x05] = '$',
	[0x06] = '%',  [0x07] = '^', [0x08] = '&',	[0x09] = '*',  [0x0A] = '(',
	[0x0B] = ')',  [0x0C] = '_', [0x0D] = '+',	[0x0E] = '\b', [0x0F] = '\t',
	[0x10] = 'Q',  [0x11] = 'W', [0x12] = 'E',	[0x13] = 'R',  [0x14] = 'T',
	[0x15] = 'Y',  [0x16] = 'U', [0x17] = 'I',	[0x18] = 'O',  [0x19] = 'P',
	[0x1A] = '{',  [0x1B] = '}', [0x1C] = '\n', [0x1E] = 'A',  [0x1F] = 'S',
	[0x20] = 'D',  [0x21] = 'F', [0x22] = 'G',	[0x23] = 'H',  [0x24] = 'J',
	[0x25] = 'K',  [0x26] = 'L', [0x27] = ':',	[0x28] = '"',  [0x29] = '~',
	[0x2B] = '|',  [0x2C] = 'Z', [0x2D] = 'X',	[0x2E] = 'C',  [0x2F] = 'V',
	[0x30] = 'B',  [0x31] = 'N', [0x32] = 'M',	[0x33] = '<',  [0x34] = '>',
	[0x35] = '?',  [0x39] = ' ',
};

static void KbdPutChar(char c)
{
	uint16_t nxt = (KbdBuffer.Head + 1) & (KbdBufSize - 1);
	if (nxt == KbdBuffer.Tail)
		return;

	KbdBuffer.Data[KbdBuffer.Head] = c;
	KbdBuffer.Head = nxt;
}

static void KbdProcessScancode(uint8_t sc)
{
	static bool ext;

	if (sc == 0xE0) {
		ext = true;
		return;
	}

	if (ext) {
		ext = false;
		return;
	}

	bool rel = (sc & 0x80) != 0;
	uint8_t key = sc & 0x7F;

	switch (key) {
	case 0x2A:
	case 0x36:
		KbdMod.Shift = !rel;
		return;
	case 0x1D:
		KbdMod.Ctrl = !rel;
		return;
	case 0x38:
		KbdMod.Alt = !rel;
		return;
	case 0x3A:
		if (!rel)
			KbdMod.Caps = !KbdMod.Caps;
		return;
	}

	if (rel)
		return;

	char c = KbdNorm[key];
	if (KbdMod.Shift)
		c = KbdShft[key];

	if (KbdMod.Caps && c >= 'a' && c <= 'z')
		c -= 32;
	else if (KbdMod.Caps && c >= 'A' && c <= 'Z')
		c += 32;

	if (c) {
		KbdPutChar(c);
		if (KbdEcho) {
			if (c == '\n') {
				StdoutPutc('\r');
				StdoutPutc('\n');
			} else if (c == '\b') {
				StdoutPutc('\b');
				StdoutPutc(' ');
				StdoutPutc('\b');
			} else {
				StdoutPutc(c);
			}
		}
	}
}

static void KbdIrqHandler(Frame *Frame)
{
	(void)Frame;
	uint8_t sc = PortIORead8(KbdData);
	KbdProcessScancode(sc);
}

static void KbdWaitWrite(void)
{
	for (int i = 0; i < 10000; ++i) {
		if (!(PortIORead8(KbdStat) & 2))
			return;
	}
}

static void KbdWaitRead(void)
{
	for (int i = 0; i < 10000; ++i) {
		if (PortIORead8(KbdStat) & 1)
			return;
	}
}

static void KbdBind(Device *Dev)
{
	(void)Dev;
	KbdBuffer.Head = 0;
	KbdBuffer.Tail = 0;
	KbdMod.Shift = KbdMod.Ctrl = KbdMod.Alt = KbdMod.Caps = false;
	KbdEcho = false;

	KbdWaitWrite();
	PortIOWrite8(KbdStat, 0xAE);

	KbdWaitWrite();
	PortIOWrite8(KbdStat, 0x20);
	KbdWaitRead();
	uint8_t cfg = PortIORead8(KbdData);
	cfg |= 0x01;

	KbdWaitWrite();
	PortIOWrite8(KbdStat, 0x60);
	KbdWaitWrite();
	PortIOWrite8(KbdData, cfg);

	for (int i = 0; i < 100; ++i) {
		if (PortIORead8(KbdStat) & 1)
			PortIORead8(KbdData);
		else
			break;
	}

	IrqRegisterHandler(IrqKeyboard, KbdIrqHandler);

	LogOk("device.kbd", "PS/2 keyboard online");
}

static int64_t KbdRead(Device *Dev, void *Buf, uint64_t Size)
{
	(void)Dev;
	char *dst = (char *)Buf;
	uint64_t n = 0;

	while (n < Size) {
		uint16_t tail = KbdBuffer.Tail;
		uint16_t head = KbdBuffer.Head;

		if (tail == head)
			break;

		dst[n++] = KbdBuffer.Data[tail];
		KbdBuffer.Tail = (tail + 1) & (KbdBufSize - 1);
	}

	return (int64_t)n;
}

static int64_t KbdWrite(Device *Dev, const void *Buf, uint64_t Size)
{
	(void)Dev;
	(void)Buf;
	(void)Size;
	return -1;
}

static int64_t KbdControl(Device *Dev, uint64_t Cmd, void *Arg)
{
	(void)Dev;
	(void)Arg;
	switch (Cmd) {
	case KbdCtrlEchoOn:
		KbdEcho = true;
		return 0;
	case KbdCtrlEchoOff:
		KbdEcho = false;
		return 0;
	default:
		return -1;
	}
}

Driver KbdDriver = {
	.Name = "ps2-kbd",
	.Bind = KbdBind,
	.Remove = 0,
	.Read = KbdRead,
	.Write = KbdWrite,
	.Control = KbdControl,
};

Device KbdDevice = {
	.Name = "kbd",
	.Drv = 0,
	.Data = 0,
};
