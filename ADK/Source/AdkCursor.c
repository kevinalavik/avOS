#include <Adk/AdkCursor.h>

#define ADK_CURSOR_FALLBACK_SIZE 5

void AdkCursorInit(AdkCursor *Cursor, AdkImage *Image)
{
	Cursor->Image = Image;
	Cursor->HotspotX = 0;
	Cursor->HotspotY = 0;
	Cursor->X = 0;
	Cursor->Y = 0;
}

void AdkCursorDraw(const AdkCursor *Cursor, AdkCanvas *Canvas)
{
	if (Cursor->Image) {
		AdkCanvasDrawImage(Canvas, Cursor->Image, Cursor->X - Cursor->HotspotX,
						   Cursor->Y - Cursor->HotspotY);
		return;
	}

	AdkRect Stem;
	Stem.X = Cursor->X;
	Stem.Y = Cursor->Y;
	Stem.Width = 1;
	Stem.Height = ADK_CURSOR_FALLBACK_SIZE;

	AdkRect Cross;
	Cross.X = Cursor->X - 2;
	Cross.Y = Cursor->Y;
	Cross.Width = ADK_CURSOR_FALLBACK_SIZE;
	Cross.Height = 1;

	AdkCanvasFillRect(Canvas, &Stem, 0xFFFFFFFFu);
	AdkCanvasFillRect(Canvas, &Cross, 0xFFFFFFFFu);
}
