#ifndef ADK_CURSOR_H
#define ADK_CURSOR_H

#include <Adk/AdkGraphics.h>

typedef struct {
	AdkImage *Image;
	S32 HotspotX;
	S32 HotspotY;
	S32 X;
	S32 Y;
} AdkCursor;

void AdkCursorInit(AdkCursor *Cursor, AdkImage *Image);
void AdkCursorDraw(const AdkCursor *Cursor, AdkCanvas *Canvas);

#endif
