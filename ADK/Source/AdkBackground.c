#include <Adk/AdkBackground.h>

void AdkBackgroundInit(AdkBackground *Background, AdkImage *Image,
					   U32 FillColor)
{
	Background->Image = Image;
	Background->FillColor = FillColor;
}

void AdkBackgroundDraw(const AdkBackground *Background, AdkCanvas *Canvas)
{
	if (!Background->Image) {
		AdkCanvasClear(Canvas, Background->FillColor);
		return;
	}

	AdkRect FullRect;
	FullRect.X = 0;
	FullRect.Y = 0;
	FullRect.Width = Canvas->Surface.Width;
	FullRect.Height = Canvas->Surface.Height;
	AdkCanvasDrawImageScaled(Canvas, Background->Image, &FullRect);
}
