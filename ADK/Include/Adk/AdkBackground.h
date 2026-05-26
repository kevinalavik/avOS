#ifndef ADK_BACKGROUND_H
#define ADK_BACKGROUND_H

#include <Adk/AdkGraphics.h>

typedef struct {
	AdkImage *Image;
	U32 FillColor;
} AdkBackground;

void AdkBackgroundInit(AdkBackground *Background, AdkImage *Image,
					   U32 FillColor);
void AdkBackgroundDraw(const AdkBackground *Background, AdkCanvas *Canvas);

#endif
