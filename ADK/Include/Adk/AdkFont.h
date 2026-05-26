#ifndef ADK_FONT_H
#define ADK_FONT_H

#include <Adk/AdkGraphics.h>

#define ADK_FONT_GLYPH_WIDTH 8u
#define ADK_FONT_GLYPH_HEIGHT 8u

void AdkFontDrawChar(AdkCanvas *Canvas, S32 X, S32 Y, char Ch, U32 Color);
void AdkFontDrawText(AdkCanvas *Canvas, S32 X, S32 Y, const char *Text,
					 U32 Color);

#endif
