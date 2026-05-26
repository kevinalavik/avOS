#include <Drivers/Display/FbConsole.h>
#include <Device/Serial.h>
#include <flanterm.h>

static struct flanterm_context *ConsoleCtx;
static bool FbAvailable;

void FbConsoleInit(struct flanterm_context *Ctx)
{
	ConsoleCtx = Ctx;
	FbAvailable = (Ctx != 0);
}

bool FbConsoleAvailable(void)
{
	return FbAvailable;
}

void ConsoleFbClaimed(void)
{
	FbAvailable = false;
}

void FbConsoleForceClaim(void)
{
	FbAvailable = true;
}

static void FbConsoleBind(Device *Dev)
{
	(void)Dev;
}

static int64_t FbConsoleWrite(Device *Dev, const void *Buf, uint64_t Size)
{
	(void)Dev;
	const char *Chars = (const char *)Buf;
	uint64_t Remaining = Size;

	while (Remaining > 0) {
		const char *Start = Chars;
		uint64_t ChunkLen = 0;

		while (Remaining > 0 && *Chars != '\n') {
			++Chars;
			++ChunkLen;
			--Remaining;
		}

		if (ChunkLen > 0) {
			if (FbAvailable && ConsoleCtx)
				flanterm_write(ConsoleCtx, Start, ChunkLen);
			SerialWrite(Start, ChunkLen);
		}

		if (Remaining > 0 && *Chars == '\n') {
			if (FbAvailable && ConsoleCtx)
				flanterm_write(ConsoleCtx, "\n\r", 2);
			SerialWrite("\r\n", 2);
			++Chars;
			--Remaining;
		}
	}

	return (int64_t)Size;
}

static int64_t FbConsoleRead(Device *Dev, void *Buf, uint64_t Size)
{
	(void)Dev;
	(void)Buf;
	(void)Size;
	return -1;
}

static int64_t FbConsoleControl(Device *Dev, uint64_t Cmd, void *Arg)
{
	(void)Dev;
	(void)Cmd;
	(void)Arg;
	return -1;
}

Driver FbConsoleDriver = {
	.Name = "fbconsole",
	.Bind = FbConsoleBind,
	.Remove = 0,
	.Read = FbConsoleRead,
	.Write = FbConsoleWrite,
	.Control = FbConsoleControl,
};

Device FbConsoleDevice = {
	.Name = "fbconsole",
	.Drv = 0,
	.Data = 0,
};
