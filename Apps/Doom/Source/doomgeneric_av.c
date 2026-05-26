#include <Adk/AdkMain.h>
#include <Adk/AdkAether.h>
#include <System/Types.h>
#include <System/Time.h>

#include "doomgeneric.h"
#include "doomkeys.h"

#define DOOM_WINDOW_WIDTH DOOMGENERIC_RESX
#define DOOM_WINDOW_HEIGHT DOOMGENERIC_RESY
#define DOOM_KEY_QUEUE_SIZE 64u
#define DOOM_FALLBACK_HOLD_FRAMES 8u

typedef struct DoomQueuedKey {
	int Pressed;
	unsigned char Key;
} DoomQueuedKey;

static AdkAppContext *DoomContext;
static Bool DoomStarted;
static Bool DoomStartFailed;
static Bool DoomRawInputSeen;
static U64 DoomStartTicks;
static DoomQueuedKey DoomKeyQueue[DOOM_KEY_QUEUE_SIZE];
static U32 DoomKeyQueueRead;
static U32 DoomKeyQueueWrite;
static U8 DoomKeyDown[256];
static U8 DoomFallbackHold[256];

static void DoomQueueRawKey(int Pressed, unsigned char Key)
{
	U32 Next;

	if (Key == 0)
		return;

	if (Pressed) {
		if (DoomKeyDown[Key])
			return;
		DoomKeyDown[Key] = 1;
	} else {
		if (!DoomKeyDown[Key])
			return;
		DoomKeyDown[Key] = 0;
	}

	Next = (DoomKeyQueueWrite + 1u) % DOOM_KEY_QUEUE_SIZE;
	if (Next == DoomKeyQueueRead)
		return;

	DoomKeyQueue[DoomKeyQueueWrite].Pressed = Pressed;
	DoomKeyQueue[DoomKeyQueueWrite].Key = Key;
	DoomKeyQueueWrite = Next;
}

static unsigned char DoomTranslateAdkKey(U16 Key, char Ch)
{
	switch (Key) {
	case AdkKeyEscape:
		return KEY_ESCAPE;
	case AdkKeyEnter:
		return KEY_ENTER;
	case AdkKeyTab:
		return KEY_TAB;
	case AdkKeyUp:
		return KEY_UPARROW;
	case AdkKeyDown:
		return KEY_DOWNARROW;
	case AdkKeyLeft:
		return KEY_LEFTARROW;
	case AdkKeyRight:
		return KEY_RIGHTARROW;
	case AdkKeySpace:
		return KEY_USE;
	case AdkKeyLeftCtrl:
	case AdkKeyRightCtrl:
		return KEY_FIRE;
	case AdkKeyLeftShift:
	case AdkKeyRightShift:
		return KEY_RSHIFT;
	default:
		break;
	}

	if (Key >= 'A' && Key <= 'Z')
		Key = (U16)(Key - 'A' + 'a');
	if (Ch >= 'A' && Ch <= 'Z')
		Ch = (char)(Ch - 'A' + 'a');

	switch (Key) {
	case 'w':
		return KEY_UPARROW;
	case 's':
		return KEY_DOWNARROW;
	case 'a':
		return KEY_LEFTARROW;
	case 'd':
		return KEY_RIGHTARROW;
	case 'j':
		return KEY_FIRE;
	case 'k':
		return KEY_USE;
	case 'l':
		return KEY_RSHIFT;
	default:
		break;
	}

	if (Ch >= 'A' && Ch <= 'Z')
		Ch = (char)(Ch - 'A' + 'a');

	switch (Ch) {
	case 0x1B:
		return KEY_ESCAPE;
	case '\n':
	case '\r':
		return KEY_ENTER;
	case '\t':
		return KEY_TAB;
	case 'w':
		return KEY_UPARROW;
	case 's':
		return KEY_DOWNARROW;
	case 'a':
		return KEY_LEFTARROW;
	case 'd':
		return KEY_RIGHTARROW;
	case 'j':
		return KEY_FIRE;
	case 'k':
	case ' ':
		return KEY_USE;
	case 'l':
		return KEY_RSHIFT;
	default:
		break;
	}

	if (Ch != 0)
		return (unsigned char)Ch;
	if (Key > 0 && Key < 256)
		return (unsigned char)Key;

	return 0;
}

static void DoomPulse(AdkAppContext *Context, const PulseMessage *Message)
{
	(void)Context;

	if (Message == 0 || Message->Type != AdkAetherMessageKeyInput ||
		Message->Size < sizeof(AdkAetherKeyInputMessage))
		return;

	AdkAetherKeyInputMessage *Input =
		(AdkAetherKeyInputMessage *)Message->Payload;
	unsigned char Key = DoomTranslateAdkKey(Input->Key, Input->Char);
	if (Key == 0)
		return;

	DoomRawInputSeen = True;
	DoomFallbackHold[Key] = 0;
	DoomQueueRawKey(Input->Pressed ? 1 : 0, Key);
}

static void DoomCharInput(AdkAppContext *Context, char Ch)
{
	(void)Context;

	if (DoomRawInputSeen)
		return;

	unsigned char Key = DoomTranslateAdkKey((U16)(U8)Ch, Ch);
	if (Key == 0)
		return;

	DoomQueueRawKey(1, Key);
	DoomFallbackHold[Key] = DOOM_FALLBACK_HOLD_FRAMES;
}

static Bool DoomInitialize(AdkAppContext *Context)
{
	DoomContext = Context;
	DoomStarted = False;
	DoomStartFailed = False;
	DoomRawInputSeen = False;
	DoomStartTicks = TimeTicks();
	DoomKeyQueueRead = 0;
	DoomKeyQueueWrite = 0;

	for (U32 Index = 0; Index < sizeof(DoomKeyDown); ++Index) {
		DoomKeyDown[Index] = 0;
		DoomFallbackHold[Index] = 0;
	}

	return True;
}

static void DoomReleaseFallbackKeys(void)
{
	if (DoomRawInputSeen) {
		for (U32 Key = 0; Key < sizeof(DoomFallbackHold); ++Key)
			DoomFallbackHold[Key] = 0;
		return;
	}

	for (U32 Key = 0; Key < sizeof(DoomFallbackHold); ++Key) {
		if (DoomFallbackHold[Key] == 0)
			continue;
		DoomFallbackHold[Key]--;
		if (DoomFallbackHold[Key] == 0)
			DoomQueueRawKey(0, (unsigned char)Key);
	}
}

static void DoomUpdate(AdkAppContext *Context)
{
	static char *Argv[] = {
		"Doom",
		"-iwad",
		"a:/Applications/Doom.app/doom1.wad",
		0,
	};

	DoomContext = Context;

	if (DoomStartFailed)
		return;

	if (Context->RawBuffer == 0)
		return;

	if (!DoomStarted) {
		DoomStarted = True;
		doomgeneric_Create(3, Argv);
	}

	doomgeneric_Tick();
	DoomReleaseFallbackKeys();
}

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
	if (DoomContext == 0 || DoomContext->RawBuffer == 0 || DG_ScreenBuffer == 0)
		return;

	U32 *Dst = (U32 *)DoomContext->RawBuffer;
	U32 DstWidth = DoomContext->RawBufferWidth;
	U32 DstHeight = DoomContext->RawBufferHeight;
	U32 CopyWidth = DstWidth < DOOMGENERIC_RESX ? DstWidth : DOOMGENERIC_RESX;
	U32 CopyHeight =
		DstHeight < DOOMGENERIC_RESY ? DstHeight : DOOMGENERIC_RESY;
	U32 XOffset =
		DstWidth > DOOMGENERIC_RESX ? (DstWidth - DOOMGENERIC_RESX) / 2u : 0u;
	U32 YOffset =
		DstHeight > DOOMGENERIC_RESY ? (DstHeight - DOOMGENERIC_RESY) / 2u : 0u;

	for (U32 Y = 0; Y < CopyHeight; ++Y) {
		U32 *DstRow = Dst + (Y + YOffset) * DstWidth + XOffset;
		pixel_t *SrcRow = DG_ScreenBuffer + Y * DOOMGENERIC_RESX;
		for (U32 X = 0; X < CopyWidth; ++X)
			DstRow[X] = 0xFF000000u | ((U32)SrcRow[X] & 0x00FFFFFFu);
	}
}

void DG_SleepMs(U32 Ms)
{
	U64 Frequency = TimeFrequency();
	U64 Start = TimeTicks();
	U64 Target = Start + ((Frequency * (U64)Ms) / 1000u);

	while (TimeTicks() < Target)
		__asm__ volatile("pause" ::: "memory");
}

U32 DG_GetTicksMs(void)
{
	U64 Frequency = TimeFrequency();
	U64 Ticks = TimeTicks();

	if (Frequency == 0)
		return 0;

	if (Ticks >= DoomStartTicks)
		Ticks -= DoomStartTicks;

	return (U32)((Ticks * 1000u) / Frequency);
}

int DG_GetKey(int *Pressed, unsigned char *DoomKey)
{
	if (DoomKeyQueueRead == DoomKeyQueueWrite)
		return 0;

	if (Pressed)
		*Pressed = DoomKeyQueue[DoomKeyQueueRead].Pressed;
	if (DoomKey)
		*DoomKey = DoomKeyQueue[DoomKeyQueueRead].Key;

	DoomKeyQueueRead = (DoomKeyQueueRead + 1u) % DOOM_KEY_QUEUE_SIZE;
	return 1;
}

void DG_SetWindowTitle(const char *Title)
{
	(void)Title;
}

int main(void)
{
	AdkApplication Application;

	Application.Title = "Doom";
	Application.WindowWidth = DOOM_WINDOW_WIDTH;
	Application.WindowHeight = DOOM_WINDOW_HEIGHT;
	Application.WindowKind = AdkAetherWindowKindRaw;
	Application.Initialize = DoomInitialize;
	Application.Update = DoomUpdate;
	Application.CharInput = DoomCharInput;
	Application.Pulse = DoomPulse;
	Application.Shutdown = 0;

	return AdkRunApplication(&Application);
}
