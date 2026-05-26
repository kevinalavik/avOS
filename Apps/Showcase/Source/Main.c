#include <Adk/AdkMain.h>
#include <System/Types.h>

#define SHOWCASE_WIDTH 640
#define SHOWCASE_HEIGHT 480

static AdkAppContext *ShowcaseContext;

static Bool ShowcaseInitialize(AdkAppContext *Context)
{
	ShowcaseContext = Context;
	return True;
}

static void ShowcaseUpdate(AdkAppContext *Context)
{
	if (!Context->RawBuffer)
		return;

	U32 Width = Context->RawBufferWidth;
	U32 Height = Context->RawBufferHeight;
	U32 *Buf = (U32 *)Context->RawBuffer;

	for (U32 Y = 0; Y < Height; Y++) {
		for (U32 X = 0; X < Width; X++) {
			U32 R = (X * 255u) / Width;
			U32 G = (Y * 255u) / Height;
			U32 B = ((X + Y) * 255u) / (Width + Height);

			Buf[Y * Width + X] = 0xFF000000u | (B << 16u) | (G << 8u) | R;
		}
	}
}

int main(void)
{
	AdkApplication Application;

	Application.Title = "Raw showcase";
	Application.WindowWidth = SHOWCASE_WIDTH;
	Application.WindowHeight = SHOWCASE_HEIGHT;
	Application.WindowKind = AdkAetherWindowKindRaw;
	Application.Initialize = ShowcaseInitialize;
	Application.Update = ShowcaseUpdate;
	Application.CharInput = 0;
	Application.Pulse = 0;
	Application.Shutdown = 0;

	return AdkRunApplication(&Application);
}